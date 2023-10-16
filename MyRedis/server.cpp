#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h>
#include <vector>
#include <string>
#include <map>
#include "constants.h"
#include "utils.h"
#include "hashtable.h"

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );})

struct Conn {
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES
    
    // buffer for reading
    size_t rbuf_size = 0;
    uint8_t rbuf[4 + K_MAX_MSG];

    // buffer for writing
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + K_MAX_MSG];
};

static void fd_set_nb(int fd) {
    // syscall for setting an fd to nonblocking mode is fcntl
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void) fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    printf("client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

// IO Helpers
/**
 * The read() syscall just returns whatever data is available in the kernel, or blocks if there is none. It’s up to the application to handle insufficient data. The read_full() function reads from the kernel until it gets exactly n bytes.
 * Likewise, the write() syscall may return successfully with partial data written if the kernel buffer is full, we must keep trying when the write() returns fewer bytes than we need.
*/

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t) rv <= n);
        n -= (size_t) rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t) rv <= n);
        n -= (size_t) rv;
        buf += rv;
    }
    return 0;
}

static void state_req(Conn *conn);
static void state_res(Conn *conn);

static int32_t parse_req(const uint8_t *data, size_t len, std::vector<std::string> &out) {
    if (len < 4) return -1;

    uint32_t argc = 0;
    memcpy(&argc, &data[0], 4);
    if (argc > K_MAX_ARGS) {
        return -1;
    }

    size_t pos = 4;
    for (uint32_t i = 0; i < argc; i++) {
        if (pos + 4 > len) {
            return -1;
        }
        uint32_t arg_len = 0;
        memcpy(&arg_len, &data[pos], 4);
        if (pos + 4 + arg_len > len) {
            return -1;
        }
        out.push_back(std::string((char*) &data[pos + 4], arg_len));
        pos += arg_len + 4;
    }

    if (pos != len) {
        // trailing garbage
        return -1;
    }
    return 0;
}

// the structure for the key
struct Entry {
    struct HNode node;
    std::string key;
    std::string value;
};

// The data structure for the key space
static struct {
    HMap db;
} g_data;


static std::map<std::string, std::string> g_map;

static bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return lhs->hcode == rhs->hcode && le->key == re->key;
}

// ====== The code for our serialization protocol ======
// TLV(type-length-value)
static void out_nil(std::string &out) {
    out.push_back(SER_NIL);
}

/**
 * note: string& append (const char* s, size_t n);
 * n: Number of characters to copy.
*/
static void out_str(std::string &out, const std::string &val) {
    // +---------+-------------+-----+------+--------
    // | SER_STR | len(4Bytes) |   val(len Bytes)
    // +---------+-------------+-----+------+--------
    out.push_back(SER_STR);
    uint32_t len = (uint32_t) val.size();
    out.append((char *)&len, 4);
    out.append(val);
}

static void out_int(std::string &out, int64_t val) {
    out.push_back(SER_INT);
    out.append((char *)&val, 8);
}

static void out_err(std::string &out, int32_t code, const std::string &msg) {
    out.push_back(SER_ERR);
    out.append((char *)&code, 4); // 4 Bytes error code
    uint32_t len = (uint32_t) msg.size();
    out.append((char *) &len, 4);
    out.append(msg);
}

static void out_arr(std::string &out, uint32_t n) {
    out.push_back(SER_ARR);
    out.append((char *)&n, 4);
}

static void do_get(
    std::vector<std::string> &cmd, 
    std::string &out) {
    
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (NULL == node) {
        return out_nil(out);
    }

    const std::string &val = container_of(node, Entry, node)->value;
   
    assert(val.size() <= K_MAX_MSG);
    return out_str(out, val);
}

static void do_set(
    std::vector<std::string> &cmd, 
    std::string &out) {

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (NULL != node) {
        std::string &val = container_of(node, Entry, node)->value;
        val.swap(cmd[2]);
    } else {
        Entry *entry = new Entry();
        entry->key.swap(key.key);
        entry->node.hcode = key.node.hcode;
        entry->value.swap(cmd[2]);
        hm_insert(&g_data.db, &(entry->node));
    }
    return out_nil(out);
}

static void do_del(
    std::vector<std::string> &cmd, 
    std::string &out) {

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (NULL != node) {
        delete container_of(node, Entry, node);
    }
    return out_int(out, node ? 1 : 0);
}

static void do_keys(std::vector<std::string> &cmd, std::string &out) {
    (void) cmd;
    out_arr(out ,(uint32_t)hm_size(&g_data.db));
    h_scan(&g_data.db.ht1, &cb_scan, &out);
    h_scan(&g_data.db.ht2, &cb_scan, &out);
}

static bool cmd_is(const std::string &word, const char * cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

static void h_scan(HTab *tab, void (*f)(HNode *, void *), void *arg) {
    if (tab->size == 0) {
        return;
    }
    for (size_t i = 0; i <= tab->mask; i++) {
        HNode *node = tab->tab[i];
        while (node != NULL) {
            f(node, arg);
            node = node->next;
        }
    }
}

static void cb_scan(HNode *node, void *arg) {
    std::string &out = *(std::string *)arg;
    out_str(out, container_of(node, Entry, node)->key);
}

/**
 * recognize get, set, del
 * @return return -1 if bad req
*/
static int32_t do_request(std::vector<std::string> &cmd, std::string &out) {
    // TODO: need to modify
    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        do_del(cmd, out);
    } else if (cmd.size() == 1 && cmd_is(cmd[0], "keys")) {
        do_keys(cmd, out);
    } else {
        // the cmd is not recognized
        out_err(out, ERR_UNKNOWN, "Unknown cmd");
    }
    return 0;
}


static int32_t try_one_request(Conn *conn) {
    // 4 bytes header, like this:
    // +-----+------+-----+------+--------
    // | len | msg1 | len | msg2 | more...
    // +-----+------+-----+------+--------
    
    if (conn->rbuf_size < 4) {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, conn->rbuf, 4);  // assume little endian
    if (len > K_MAX_MSG) {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }
    if (4 + len > conn->rbuf_size) {
        // not enough data in the buffer. Will retry in the next iteration
        return false;
    }

    // parse the request
    std::vector<std::string> cmd;
    if (0 != parse_req(&conn->rbuf[4], len, cmd)) {
        msg("bad req");
        conn->state = STATE_END;
        return false;
    }

    // got one request
    std::string out;
    do_request(cmd, out);

    // pack the response into the buffer
    if (4 + out.size() > K_MAX_MSG) {
        out.clear();
        out_err(out, ERR_2BIG, "response is too big");
    }

    // generate the response
    uint32_t rescode = 0;
    uint32_t wlen = (uint32_t)out.size();

    memcpy(conn->wbuf, &wlen, 4);
    memcpy(&conn->wbuf[4], out.data(), out.size());
    conn->wbuf_size = wlen + 4;

    // remove the request from the buffer
    // note: frequent memmove is inefficient.
    // note: need better handling for production code
    size_t remain_size = conn->rbuf_size - (len + 4);
    if (remain_size > 0) {
        memmove(conn->rbuf, conn->rbuf + len + 4, remain_size);
    }
    conn->rbuf_size = remain_size;

    // update state(STARE_RES)
    conn->state = STATE_RES;
    state_res(conn);

    return (conn->state == STATE_REQ);
}

/**
 * fd2conn[conn->fd] = conn;
*/
static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn) {
    if (fd2conn.size() <= (size_t) conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

/**
 * accepts a new connection and creates the struct Conn object
 * @return 0 if accept successfully, else -1 
*/
static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *) &client_addr, &socklen);
    if (connfd < 0) {
        msg("accept() error");
        return -1;
    }

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);
    // creating the struct Conn
    struct Conn *conn = (struct Conn *) malloc(sizeof(struct Conn));
    if (!conn) {
        close(connfd);
        return -1;
    }
    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = conn->wbuf_size = conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);
    return 0; // success
}



static bool try_fill_buffer(Conn *conn) {
    // try to fill the buffer
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;

    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }
    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            msg("unexpected EOF");
        } else {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t) rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));
    
    // Try to process requests one by one 
    while (try_one_request(conn)) {}
    return (conn->state == STATE_REQ);
}

static bool try_flush_buffer(Conn *conn) {
    ssize_t rv = 0;
    do {
        size_t remain_size = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, conn->wbuf + conn->wbuf_sent, remain_size);
    } while (rv < 0 && errno == EINTR); /* Interrupted system call */

    if (rv < 0 && errno == EAGAIN) /* Try again */ {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }

    conn->wbuf_sent += (size_t) rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);

    if (conn->wbuf_sent == conn->wbuf_size) {
        // fully sent
        conn->wbuf_sent = conn->wbuf_size = 0;
        conn->state = STATE_REQ;
        return false; // needn't write
    }

    // still got some data in wbuf, could try to write again
    return true;
}

static void state_req(Conn *conn) {
    while (try_fill_buffer(conn)) {}
}

static void state_res(Conn *conn) {
    while(try_flush_buffer(conn)) {}
}

static void connection_io(Conn *conn) {
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0); // not expect
    }
}

int main() {
    // 1. Obtain a socket fd, AF_INET is for IPv4, SOCK_STREAM is for TCP
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    
    // this is nedded for most server applications
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // 2. bind, this is the syntax that deals with IPv4 addresses
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // 3. listen
    rv = listen(fd, SOMAXCONN);
        if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

    // the event loop
    std::vector<struct pollfd> poll_args;
    while (true) {
        // prepare the arguments of the poll()
        poll_args.clear();
        // for convenience, the listening fd is put in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        // connection fds
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }

            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.events |= POLLERR;
            poll_args.push_back(pfd);
        }

        // poll for active fds
        // the timeout argument doesn't matter here
        int rv = poll(poll_args.data(), (nfds_t) poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }

        // process active connections
        for (size_t i = 1; i < poll_args.size(); i++) {
            if (poll_args[i].revents) {
                Conn *conn = fd2conn[poll_args[i].fd];
                connection_io(conn);

                if (conn->state == STATE_END) {
                    // client closed normally, or something bad happened.
                    // destroy this connection
                    fd2conn[conn->fd] = NULL; // delete it
                    (void) close(conn->fd);
                    free(conn);
                }
            }
        }

        // try to accept a new connection if the listening fd is active
        if (poll_args[0].revents) {
            (void) accept_new_conn(fd2conn, fd);
        }
    }

    return 0;
}