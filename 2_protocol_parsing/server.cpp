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
#include "constants.h"
#include "utils.h"

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

static int32_t one_request(int connfd) {
    // 4 bytes header, like this:
    // +-----+------+-----+------+--------
    // | len | msg1 | len | msg2 | more...
    // +-----+------+-----+------+--------
    char rbuf[4 + K_MAX_MSG + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    
    if (err) {   // read_full return -1
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);  // assume little endian
    if (len > K_MAX_MSG) {
        msg("too long");
        return -1;
    }

    // request body
    err = read_full(connfd, rbuf + 4, len);
    if (err) {
        msg("read() error");
        return err;
    }

    // do something
    rbuf[len + 4] = '\0';
    printf("client says: %s\n", rbuf + 4);

    // reply using the same protocol
    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    len = (uint32_t) strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(wbuf + 4, reply, len);
    return write_all(connfd, wbuf, len + 4);
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


    // Loop for each connection and do something with them 
    while(true) {
        // 4. accept
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
        if (connfd < 0) {
            continue;     // error 
        }

        // only serves one client connection at once
        while(true) {
            int32_t err = one_request(connfd);
            if (err) {
                break;
            }
        }
        close(connfd);
    }
    return 0;
}