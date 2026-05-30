// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin
//
// file-age core — hand-written business logic.
// Called by generated scaffold (scaffold handles --help, config, DB init).

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <cjson/cJSON.h>
#include "vehir_lib.h"

static double now_unix(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        vl_die("file-age", "clock_gettime failed");
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    size_t cap = 8192;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) { fclose(f); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); fclose(f); return NULL; }
            buf = tmp;
        }
    }
    int err = ferror(f);
    fclose(f);
    if (err) { free(buf); return NULL; }

    buf[len] = '\0';
    return buf;
}

static void print_result(int ok, double age, double max_age,
                          const char *file, const char *field) {
    cJSON *root = cJSON_CreateObject();
    if (!root) vl_die("file-age", "cJSON alloc failed");
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddNumberToObject(root, "age_seconds", age);
    cJSON_AddNumberToObject(root, "max_age_seconds", max_age);
    cJSON_AddStringToObject(root, "file", file);
    cJSON_AddStringToObject(root, "field", field);

    char *s = cJSON_PrintUnformatted(root);
    if (!s) { cJSON_Delete(root); vl_die("file-age", "cJSON print failed"); }
    printf("%s\n", s);
    free(s);
    cJSON_Delete(root);
}

static int cmd_check(const char *path, double max_age, const char *field) {
    char *buf = read_file(path);
    if (!buf)
        vl_die("file-age", "file not found or unreadable");

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root)
        vl_die("file-age", "invalid JSON");

    cJSON *ts_item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (!ts_item || !cJSON_IsNumber(ts_item)) {
        cJSON_Delete(root);
        vl_die("file-age", "timestamp field not found or not a number");
    }

    double ts = ts_item->valuedouble;
    cJSON_Delete(root);

    if (ts <= 0.0)
        vl_die("file-age", "timestamp value <= 0");

    double t = now_unix();
    double age = t - ts;
    if (age < 0.0) age = 0.0;

    int ok = (age <= max_age);
    print_result(ok, age, max_age, path, field);
    return ok ? 0 : 1;
}

int file_age_core(vehir_db *db, int argc, char **argv, int arg_off) {
    (void)db;

    if (arg_off >= argc)
        usage(argv[0]);

    if (strcmp(argv[arg_off], "check") != 0) {
        fprintf(stderr, "file-age: unknown command '%s'\n", argv[arg_off]);
        return 2;
    }

    if (arg_off + 3 > argc)
        usage(argv[0]);

    const char *path = argv[arg_off + 1];
    char *end = NULL;
    double max_age = strtod(argv[arg_off + 2], &end);
    if (end == argv[arg_off + 2] || max_age < 0.0) {
        fprintf(stderr, "file-age: invalid max_age_seconds '%s'\n", argv[arg_off + 2]);
        return 2;
    }

    const char *field = (arg_off + 4 <= argc) ? argv[arg_off + 3] : "timestamp";
    return cmd_check(path, max_age, field);
}
