// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_fs.c — filesystem wrappers for spec2c-generated code
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
#include "ipm_builtins.h"

/* directory_list(path) → cJSON array of filenames, or NULL on error */
cJSON* directory_list(const char *path) {
    if (!path) return NULL;
    DIR *d = opendir(path);
    if (!d) return NULL;
    cJSON *arr = cJSON_CreateArray();
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip hidden */
        cJSON_AddItemToArray(arr, cJSON_CreateString(entry->d_name));
    }
    closedir(d);
    return arr;
}

/* file_exists(path) → 1 if regular file, 0 otherwise */
int file_exists(const char *path) {
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

/* dir_exists(path) → 1 if directory, 0 otherwise */
int dir_exists(const char *path) {
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}
