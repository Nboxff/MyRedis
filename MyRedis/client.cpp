#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h>
#include <vector>
#include <string>
#include "constants.h"
#include "utils.h"

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

static int32_t send_req(int fd, const std::vector<std::string> &cmd) {
    uint32_t len = 4;
    uint32_t n = cmd.size();
    for (const std::string &s: cmd) {
        len += s.size() + 4;
    }
    if (len > K_MAX_ARGS) {
        return -1;
    }

    char wbuf[4 + K_MAX_MSG];
    memcpy(wbuf, &len, 4);
    memcpy(wbuf + 4, &n, 4);
    size_t cur = 8;
    for (const std::string &s : cmd) {
        uint32_t p = (uint32_t) s.size();
        memcpy(wbuf + cur, &p, 4);
        memcpy(wbuf + cur + 4, s.data(), s.size());
        cur += 4 + s.size();
    }
    return write_all(fd, wbuf, len + 4);
}

static int32_t on_response(const uint8_t *data, size_t size) {
    if (size < 1) {
        msg("bad response");
        return -1;
    }
    switch (data[0]) {
    case SER_NIL:
        printf("(nil)\n");
        return 1;
    case SER_ERR:
        if (size < 1 + 8) {
            msg("bad response");
            return -1;
        }
        int32_t code = 0;
        uint32_t len = 0;
        memcpy(&code, data + 1, 4);
        memcpy(&len, data + 1 + 4, 4);
        if (size < 1 + 8 + len) {
            msg("bad response");
            return -1;
        }
        printf("(err) %d %.*s\n", code, len, &data[1 + 8]);
        return 1 + 8 + len;
    default:
        break;
    }
}

static int32_t read_res(int fd) {
    char rbuf[4 + K_MAX_MSG + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > K_MAX_MSG) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    // print the result
    uint32_t rescode = 0;
    if (len < 4) {
        msg("bad respinse");
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);
    printf("server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
    return 0;
}

int main(int argc, char **argv) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK); // 127.0.0.1

    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("connect");
    }

    std::vector<std::string> cmd;
    // argv[0] is ./client
    for (int i = 1; i < argc; i++) {
        cmd.push_back(argv[i]);
    }

    int32_t err = send_req(fd, cmd);
    if (err) {
        goto L_DONE;
    }
    err = read_res(fd);
    if (err) {
        goto L_DONE;
    }

    // ====== Test for multiple pipelined requests ======
    // Test for multiple pipelined requests
    // const char *query_list[3] = {"hello1", "hello2", "hello3"};
    // const int query_n = 3;
    // for (size_t i = 0; i < query_n; i++) {
    //     int32_t err = send_req(fd, query_list[i]);
    //     if (err) {
    //         goto L_DONE;
    //     }
    // }

    // for (size_t i = 0; i < query_n; i++) {
    //     int32_t err = read_res(fd);
    //     if (err) {
    //         goto L_DONE;
    //     }
    // }
    // ==================================================

L_DONE:
    close(fd);
    return 0;
}