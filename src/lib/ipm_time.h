// SPDX-License-Identifier: Apache-2.0
// ipm_time.h — high-precision timing for all tools
// Include this, call ipm_time_init() at start, ipm_time_us() to get elapsed microseconds
#ifndef IPM_TIME_H
#define IPM_TIME_H
#include <time.h>
#include <stdint.h>

static struct timespec _ipm_t0;

static inline void ipm_time_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &_ipm_t0);
}

static inline int64_t ipm_time_us(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - _ipm_t0.tv_sec) * 1000000LL +
           (now.tv_nsec - _ipm_t0.tv_nsec) / 1000;
}

#endif
