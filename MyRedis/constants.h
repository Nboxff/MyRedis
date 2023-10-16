#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include <stdio.h>
const size_t K_MAX_MSG = 4096;

const size_t K_MAX_ARGS = 1024;

enum {
    STATE_REQ = 0,  // reading request
    STATE_RES = 1,  // sending response
    STATE_END = 2,
};

enum {
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

enum {
    SER_NIL = 0, // Like NULL
    SER_ERR = 1, // An error code and message 
    SER_STR = 2, // A string
    SER_INT = 3, // A int64
    SER_ARR = 4, // Array
};

enum {
    ERR_UNKNOWN = 1,
    ERR_2BIG = 2,
};
#endif