#include "utils.h"
#include <stdint.h>

// ====== error message tools ======
void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

void die(const char *msg) {
    int err = errno;  // error code
    fprintf(stderr, "[%d] %s\n", err, msg);

    // Please exec `man abort`
    abort();          
}


// ====== math and hash tools ======
uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}