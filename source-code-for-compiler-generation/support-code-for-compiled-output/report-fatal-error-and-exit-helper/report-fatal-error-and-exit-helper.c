#include <stdio.h>
#include <stdlib.h>

void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
}
