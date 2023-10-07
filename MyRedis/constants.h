#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include <stdio.h>
const size_t K_MAX_MSG = 4096;

enum {
    STATE_REQ = 0,  // reading request
    STATE_RES = 1,  // sending response
    STATE_END = 2,
};

#endif