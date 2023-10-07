#include "utils.h"

void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

void die(const char *msg) {
    int err = errno;  // error code
    fprintf(stderr, "[%d] %s\n", err, msg);

    // Please exec `man abort`
    abort();          
}