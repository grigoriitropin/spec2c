// SPDX-License-Identifier: Apache-2.0
// FFI exports — thin wrappers for IPM enforcer to call C enforcement functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *read_entire_file_into_string(const char *path);

int count_lines_inside_file_ffi(const char *path) {
    char *content = read_entire_file_into_string(path);
    if (!content) return -1;
    int lines = 0;
    for (const char *p = content; *p; p++)
        if (*p == '\n') lines++;
    free(content);
    return lines;
}
