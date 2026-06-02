// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_fs.c — filesystem wrappers for spec2c-generated code
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
#include "ipm_builtins.h"

/* enumerate_directory_entry_list_items(path) → cJSON array of filenames, or NULL on error */
cJSON* enumerate_directory_entry_list_items(const char *path) {
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

/* check_file_exists_upon_disk(path) → 1 if regular file, 0 otherwise */
int check_file_exists_upon_disk(const char *path) {
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

/* check_directory_exists_upon_disk(path) → 1 if directory, 0 otherwise */
int check_directory_exists_upon_disk(const char *path) {
    if (!path) return 0;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

/* count_newlines_within_text_string(text) → number of \n characters */
int count_newlines_within_text_string(const char *text) {
    if (!text) return 0;
    int n = 0;
    for (const char *p = text; *p; p++) if (*p == '\n') n++;
    return n;
}

/* file_read_bytes(path) → heap-allocated string of entire file content, or NULL on error */
char* file_read_bytes(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)fsize + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[n] = 0;
    return buf;
}

/* write_bytes_into_file_path(path, data) — overwrite file with string content */
void write_bytes_into_file_path(const char *path, const char *data) {
    if (!path || !data) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fputs(data, f);
    fclose(f);
}

/* rename_file_from_old_path(old_path, new_path) → 0 on success, -1 on error */
int rename_file_from_old_path(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return -1;
    return rename(old_path, new_path);
}

/* delete_file_from_disk_storage(path) → 0 on success, -1 on error */
int delete_file_from_disk_storage(const char *path) {
    if (!path) return -1;
    return unlink(path);
}
