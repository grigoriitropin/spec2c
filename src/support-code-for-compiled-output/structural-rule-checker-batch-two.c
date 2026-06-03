// SPDX-License-Identifier: Apache-2.0
// Batch 2 FFI exports — structural rules for IPM enforcer
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

extern char *read_entire_file_into_string(const char *path);

/* ── function count (detect function definition lines) ───────────────── */
const char *check_function_count_limit_ffi(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    int count = 0;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        const char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s && *s != '/' && *s != '*' && *s != '#' && *s != '\n' && *s < '0') {
            if (!strstr(s, "typedef") && !strstr(s, "struct") && !strstr(s, "enum")) {
                const char *op = strchr(s, '(');
                const char *ob = strrchr(s, '{');
                if (op && ob && op < ob) {
                    const char *cp = strchr(op, ')');
                    if (cp && cp < ob) {
                        count++;
                    }
                }
            }
        }
        if (!next) break;
        line = next + 1;
    }
    free(c);
    if (count > 10) return "too many functions";
    return NULL;
}

/* ── function length ─────────────────────────────────────────────────── */
const char *check_function_length_limit_ffi(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    char *line = c, *next;
    int in_func = 0, func_lines = 0, depth = 0;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        if (!in_func) {
            const char *s = line;
            while (*s == ' ' || *s == '\t') s++;
            if (*s && *s != '/' && *s != '*' && *s != '#' && *s < '0') {
                if (!strstr(s, "typedef") && !strstr(s, "struct") && !strstr(s, "enum")) {
                    const char *op = strchr(s, '(');
                    const char *ob = strrchr(s, '{');
                    if (op && ob && op < ob) {
                        const char *cp = strchr(op, ')');
                        if (cp && cp < ob) {
                            in_func = 1;
                            func_lines = 1;
                            for (const char *p = line; *p; p++) {
                                if (*p == '{') depth++;
                                else if (*p == '}') depth--;
                            }
                            if (depth <= 0) in_func = 0;
                        }
                    }
                }
            }
        } else {
            func_lines++;
            for (const char *p = line; *p; p++) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
            }
            if (depth <= 0) {
                if (func_lines > 50) { free(c); return "function too long"; }
                in_func = 0;
            }
        }
        if (!next) break;
        line = next + 1;
    }
    free(c);
    return NULL;
}

/* ── files per directory ─────────────────────────────────────────────── */
const char *check_files_per_directory_ffi(const char *dirpath) {
    DIR *d = opendir(dirpath);
    if (!d) return NULL;
    int cnt = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        size_t nl = strlen(de->d_name);
        if (nl > 2 && (!strcmp(de->d_name + nl - 2, ".c") || !strcmp(de->d_name + nl - 2, ".h")))
            cnt++;
    }
    closedir(d);
    if (cnt > 3) return "too many files in directory";
    return NULL;
}
