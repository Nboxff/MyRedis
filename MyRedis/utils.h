#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

void msg(const char *msg);

void die(const char *msg);

uint64_t str_hash(const uint8_t *data, size_t len);

#endif