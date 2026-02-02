#ifndef SF_TIME_H
#define SF_TIME_H

#include <errno.h>
#include <stdint.h>
#include <time.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns) {
    struct timespec req;
    req.tv_sec = (time_t)(ns / 1000000000ull);
    req.tv_nsec = (long)(ns % 1000000000ull);
    // retry with remaining time
    while (nanosleep(&req, &req) != 0 && errno == EINTR);
}

#endif
