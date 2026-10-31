#ifndef TIMING_H
#define TIMING_H
#include <time.h>

static inline long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000L) + (ts.tv_nsec / 1000000L);
}
#endif