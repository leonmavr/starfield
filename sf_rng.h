#ifndef SF_RNG_H
#define SF_RNG_H

#include <stdint.h>

// Linear congruential generator originally written by @Skeeto
enum { XRAND_MAX = 0x7fffffff };
static uint64_t xrandom_state = 1234;

static void xrandom_seed(uint64_t seed) {
    xrandom_state = seed;
}

static int xrandom(void) {
    xrandom_state = xrandom_state * 0x3243f6a8885a308d + 1;
    return (int)(xrandom_state >> 33);
}

#endif
