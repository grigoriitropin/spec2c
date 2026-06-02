// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin

#define _POSIX_C_SOURCE 200809L
#include "vehir-shared-abstraction-wrapper-code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cjson/cJSON.h>

// -------- error output --------

_Noreturn void vehir_fatal_error_and_exit(const char *tool, const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "%s: FATAL: cJSON alloc failed\n", tool);
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", error);
    char *s = cJSON_PrintUnformatted(root);
    if (s) {
        printf("%s\n", s);
        free(s);
    } else {
        printf("{\"ok\":false,\"error\":\"%s\"}\n", error);
    }
    cJSON_Delete(root);
    fprintf(stderr, "%s: %s\n", tool, error);
    exit(1);
}

// -------- config --------

char *resolve_default_config_file_path(void) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) return NULL;
    char *path = malloc(strlen(home) + 32);
    if (!path) return NULL;
    sprintf(path, "%s/.config/vehir/env", home);
    return path;
}

char *load_vehir_configuration_from_file(const char *tool, const char *path, const char *key) {
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    if (st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {
        fprintf(stderr, "%s: config %s is readable by group/other (mode %04o)\n",
                tool, path, st.st_mode & 0777);
        return NULL;
    }

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    size_t keylen = strlen(key);
    char line[2048];
    char *result = NULL;

    while (fgets(line, (int)sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        if (strncmp(p, key, keylen) != 0) continue;
        char *eq = p + keylen;
        while (*eq == ' ' || *eq == '\t') eq++;
        if (*eq != '=') continue;
        char *val = eq + 1;
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r' ||
                             val[vlen - 1] == ' '  || val[vlen - 1] == '\t'))
            val[--vlen] = '\0';
        result = strdup(val);
        break;
    }
    fclose(f);
    return result;
}

char *require_config_value_or_die(const char *tool, const char *path, const char *key) {
    char *val = load_vehir_configuration_from_file(tool, path, key);
    if (!val) {
        char msg[384];
        snprintf(msg, sizeof(msg), "key %s not found in config %s", key, path);
        vehir_fatal_error_and_exit(tool, msg);
    }
    return val;
}

// -------- DB helpers (thin wrappers — real impl needs db.h) --------

#ifndef VEHIR_HAS_DB

void check_database_connection_is_alive(const char *tool, vehir_db *db) {
    (void)db;
    vehir_fatal_error_and_exit(tool, "DB support not compiled in (link vehir_lib_db.c)");
}

cJSON *query_database_and_return_json(const char *tool, vehir_db *db,
                         const char *sql,
                         const char *const *params, int nparams) {
    (void)db; (void)sql; (void)params; (void)nparams;
    vehir_fatal_error_and_exit(tool, "DB support not compiled in (link vehir_lib_db.c)");
    return NULL;
}

#endif

// -------- safe exec --------

int execute_command_with_argument_list(char *const argv[]) {
    if (!argv || !argv[0]) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    int wstatus;
    if (waitpid(pid, &wstatus, 0) < 0) return -1;

    if (WIFEXITED(wstatus))
        return WEXITSTATUS(wstatus);
    return -1;
}
