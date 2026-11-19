#pragma once
#include <stdint.h>
#define WRITE64LE(P, V)                                                        \
    do {                                                                       \
        uint64_t __v = (uint64_t)(V);                                          \
        unsigned char *__p = (unsigned char *)(P);                             \
        for (int __i = 0; __i < 8; ++__i) __p[__i] = (unsigned char)(__v >> (8 * __i)); \
    } while (0)
#define READ16LE(P) ((uint16_t)(((const unsigned char *)(P))[0] | ((const unsigned char *)(P))[1] << 8))
#define READ32LE(P)                                                                  \
    ((uint32_t)((const unsigned char *)(P))[0] | (uint32_t)((const unsigned char *)(P))[1] << 8 | \
     (uint32_t)((const unsigned char *)(P))[2] << 16 | (uint32_t)((const unsigned char *)(P))[3] << 24)
#define READ64LE(P) ((uint64_t)READ32LE(P) | (uint64_t)READ32LE((const unsigned char *)(P) + 4) << 32)
