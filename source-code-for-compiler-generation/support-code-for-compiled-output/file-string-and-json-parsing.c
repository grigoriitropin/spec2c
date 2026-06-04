// SPDX-License-Identifier: Apache-2.0
// ipm_builtins — I/O, JSON, error, regex, type mapping (split part 1)
#include "../runtime-for-generated-ipm-code.h"
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#undef strlen
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

string read_entire_file_into_string(const char *path) {
    FILE *f = (!path || !path[0]) ? stdin : fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) { if (f != stdin) fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)size, f);
    buffer[n] = '\0';
    if (f != stdin) fclose(f);
    return buffer;
}

Slice read_entire_file_into_slice(const char *path) {
    Slice sl = {NULL, 0};
    FILE *f = (!path || !path[0]) ? stdin : fopen(path, "rb");
    if (!f) return sl;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        if (f != stdin) fclose(f);
        return sl;
    }
    uint8_t *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        if (f != stdin) fclose(f);
        return sl;
    }
    size_t n = fread(buffer, 1, (size_t)size, f);
    buffer[n] = '\0';
    if (f != stdin) fclose(f);
    sl.data = buffer;
    sl.len = (uint32_t)n;
    return sl;
}


void write_text_string_into_file(const char *path, const char *content) {
    if (!path || !content) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}


void terminate_with_json_error_output(const char *function_name, const char *instruction_index_str,
              const char *error_msg, const char *fix_hint) {
    cJSON *r = cJSON_CreateObject();
    if (!r) { fprintf(stderr, "{\"ok\":false,\"error\":\"cJSON alloc failed\"}\n"); exit(1); }
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", error_msg ? error_msg : "unknown error");
    if (function_name && function_name[0])
        cJSON_AddStringToObject(r, "function", function_name);
    if (instruction_index_str && instruction_index_str[0])
        cJSON_AddNumberToObject(r, "instruction_index", atoi(instruction_index_str));
    if (fix_hint && fix_hint[0])
        cJSON_AddStringToObject(r, "fix_hint", fix_hint);
    char *s = cJSON_PrintUnformatted(r);
    if (s) { fprintf(stderr, "%s\n", s); free(s); }
    cJSON_Delete(r);
    exit(1);
}


int compare_slice_data_against_bytes(const uint8_t *data, uint32_t len, const char *pattern) {
    uint32_t plen = 0;
    while (pattern[plen]) plen++;
    if (len != plen) return 1;
    for (uint32_t i = 0; i < len; i++)
        if (data[i] != (uint8_t)pattern[i]) return 1;
    return 0;
}

int match_pattern_against_text_string(Slice text, const char *pattern) {
    if (!text.data || !pattern) return 0;
    size_t pat_len = 0;
    while (pattern[pat_len]) pat_len++;
    char *unescaped = malloc(pat_len + 1);
    if (!unescaped) return 0;
    size_t u_len = 0;
    for (size_t i = 0; i < pat_len; i++) {
        if (pattern[i] == '\\' && i + 1 < pat_len) {
            unescaped[u_len++] = pattern[++i];
        } else {
            unescaped[u_len++] = pattern[i];
        }
    }
    unescaped[u_len] = '\0';
    int found = 0;
    if (u_len <= text.len) {
        for (uint32_t i = 0; i <= text.len - u_len; i++) {
            int match = 1;
            for (size_t j = 0; j < u_len; j++) {
                if (text.data[i + j] != (uint8_t)unescaped[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                found = 1;
                break;
            }
        }
    }
    free(unescaped);
    return found;
}


const char *resolve_spec_type_into_lang(const char *t) {
    if (!t) return "void *";
    if (!strcmp(t, "slice")) return "Slice";
    if (!strcmp(t, "i32")) return "int";
    if (!strcmp(t, "i64")) return "long long";
    if (!strcmp(t, "str")) return "char *";
    if (!strcmp(t, "u8")) return "uint8_t";
    if (!strcmp(t, "void")) return "void";
    if (!strcmp(t, "string")) return "char *";
    if (!strcmp(t, "int")) return "int";
    if (!strcmp(t, "float")) return "float";
    if (!strcmp(t, "boolean")) return "int";
    if (!strcmp(t, "json_object")) return "cJSON *";
    if (!strcmp(t, "json_array")) return "cJSON *";
    if (!strcmp(t, "db_handle")) return "struct vehir_db *";
    if (!strcmp(t, "subst_table")) return "subst_table *";
    if (!strcmp(t, "string_buffer")) return "string_buffer *";
    if (!strcmp(t, "file_handle")) return "FILE *";
    if (!strcmp(t, "u8_ptr")) return "const uint8_t *";
    if (!strcmp(t, "u32")) return "uint32_t";
    if (!strcmp(t, "i32")) return "int";
    if (!strcmp(t, "i64")) return "long long";
    if (!strcmp(t, "str")) return "char *";
    if (!strcmp(t, "u8")) return "uint8_t";
    return "void *";
}

cJSON *list_files_inside_directory_path(const char *path) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    DIR *d = opendir(path);
    if (!d) { cJSON_Delete(arr); return NULL; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        cJSON *entry = cJSON_CreateObject();
        if (!entry) continue;
        cJSON_AddStringToObject(entry, "name", de->d_name);
        cJSON_AddStringToObject(entry, "path", full);
        cJSON_AddBoolToObject(entry, "is_dir", S_ISDIR(st.st_mode));
        cJSON_AddItemToArray(arr, entry);
    }
    closedir(d);
    return arr;
}

/* Recursive variant — collects files from subdirectories into flat array */
cJSON *list_files_under_path_recursively(const char *path) {
    cJSON *arr = list_files_inside_directory_path(path);
    if (!arr) return NULL;
    cJSON *entry = NULL; int idx = 0;
    while ((entry = cJSON_GetArrayItem(arr, idx)) != NULL) {
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "is_dir"))) {
            const char *sub = cJSON_GetObjectItemCaseSensitive(entry, "path")->valuestring;
            cJSON *sub_arr = list_files_under_path_recursively(sub);
            if (sub_arr) {
                for (int si = 0; si < cJSON_GetArraySize(sub_arr); si++)
                    cJSON_AddItemToArray(arr, cJSON_Duplicate(cJSON_GetArrayItem(sub_arr, si), 1));
                cJSON_Delete(sub_arr);
            }
        }
        idx++;
    }
    return arr;
}

