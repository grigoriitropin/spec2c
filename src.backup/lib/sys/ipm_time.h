// SPDX-License-Identifier: Apache-2.0
// ipm_time.h — high-precision timing + timestamps for all tools
#ifndef IPM_TIME_H
#define IPM_TIME_H
#include <time.h>
#include <stdint.h>
#include <stdio.h>

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

/* ISO-8601 timestamp: "2026-06-02T05:28:14Z" */
static inline void ipm_timestamp(char buf[21]) {
    time_t t = time(NULL);
    struct tm *tm = gmtime(&t);
    snprintf(buf, 21, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
}

#endif
