// SPDX-License-Identifier: Apache-2.0
// Batch 5 FFI — cross-file rules: dead code, header include frequency
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

extern char *read_entire_file_into_string(const char *path);

/* ── collect function names from a file ─────────────────────────────── */
typedef struct { char name[128]; } fn_entry;

static int collect_function_names_from_file(const char *path, fn_entry *fns, int *count, int max) {
    char *c = read_entire_file_into_string(path);
    if (!c) return 0;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        const char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s && *s != '/' && *s != '*' && *s != '#' && *s < '0') {
            if (!strstr(s, "typedef") && !strstr(s, "struct") && !strstr(s, "enum")) {
                const char *op = strchr(s, '(');
                const char *ob = strrchr(s, '{');
                if (op && ob && op < ob) {
                    const char *cp = strchr(op, ')');
                    if (cp && cp < ob && *count < max) {
                        const char *id_end = op;
                        while (id_end > s && (*(id_end-1)==' '||*(id_end-1)=='\t'||*(id_end-1)=='*')) id_end--;
                        const char *id_start = id_end;
                        while (id_start > s && *(id_start-1)!=' '&&*(id_start-1)!='\t'&&*(id_start-1)!='*') id_start--;
                        size_t nl = (size_t)(id_end - id_start);
                        if (nl > 0 && nl < 127) {
                            memcpy(fns[*count].name, id_start, nl);
                            fns[*count].name[nl] = 0;
                            (*count)++;
                        }
                    }
                }
            }
        }
        if (!next) break;
        line = next + 1;
    }
    free(c);
    return 1;
}

/* ── check if function name is called anywhere in text ──────────────── */
static int find_function_calls_within_files(const char *path, const char *func_name) {
    char *c = read_entire_file_into_string(path);
    if (!c) return 0;
    char call[256]; snprintf(call, sizeof(call), "%s(", func_name);
    int found = strstr(c, call) != NULL;
    free(c);
    return found;
}

/* ── dead code detection (cross-file) ───────────────────────────────── */
const char *check_dead_code_across_files(const char *dirpath) {
    fn_entry fns[512]; int fn_count = 0;
    /* collect all function definitions */
    void scan_dir_fns(const char *dp) {
        DIR *d = opendir(dp);
        if (!d) return;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dp, de->d_name);
            struct stat st;
            if (stat(sub, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) { scan_dir_fns(sub); continue; }
            size_t nl = strlen(de->d_name);
            if (nl < 3 || (strcmp(de->d_name+nl-2,".c") && strcmp(de->d_name+nl-2,".h"))) continue;
            collect_function_names_from_file(sub, fns, &fn_count, 512);
        }
        closedir(d);
    }
    scan_dir_fns(dirpath);

    /* check each function for calls */
    for (int i = 0; i < fn_count; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        void search_calls(const char *dp) {
            DIR *d = opendir(dp);
            if (!d || called) { if (d) closedir(d); return; }
            struct dirent *de;
            while ((de = readdir(d)) != NULL && !called) {
                if (de->d_name[0] == '.') continue;
                char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dp, de->d_name);
                struct stat st;
                if (stat(sub, &st) != 0) continue;
                if (S_ISDIR(st.st_mode)) { search_calls(sub); continue; }
                size_t nl = strlen(de->d_name);
                if (nl < 3 || (strcmp(de->d_name+nl-2,".c") && strcmp(de->d_name+nl-2,".h"))) continue;
                if (find_function_calls_within_files(sub, fns[i].name)) called = 1;
            }
            closedir(d);
        }
        search_calls(dirpath);
        if (!called) {
            static char err[256];
            snprintf(err, sizeof(err), "dead code: '%s' never called", fns[i].name);
            return err;
        }
    }
    return NULL;
}

/* ── header include frequency (cross-file) ──────────────────────────── */
typedef struct {
    char name[128];
    int count;
} hdr_entry;

const char *check_header_include_frequency_count(const char *dirpath) {
    hdr_entry hdrs[128]; int hdr_count = 0;
    void scan_dir_hdrs(const char *dp) {
        DIR *d = opendir(dp);
        if (!d) return;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dp, de->d_name);
            struct stat st;
            if (stat(sub, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) { scan_dir_hdrs(sub); continue; }
            size_t nl = strlen(de->d_name);
            if (nl < 3 || (strcmp(de->d_name+nl-2,".c") && strcmp(de->d_name+nl-2,".h"))) continue;
            char *c = read_entire_file_into_string(sub);
            if (!c) continue;
            char *line = c, *next;
            while (line && *line) {
                next = strchr(line, '\n');
                if (next) *next = 0;
                char hdr[128] = {0};
                if (sscanf(line, " #include \"%127[^\"]\"", hdr) == 1) {
                    int found = 0;
                    for (int i = 0; i < hdr_count; i++)
                        if (!strcmp(hdrs[i].name, hdr)) {
                        hdrs[i].count++;
                        found = 1;
                        break;
                    }
                    if (!found && hdr_count < 128) {
                        snprintf(hdrs[hdr_count].name, 128, "%s", hdr);
                        hdrs[hdr_count].count = 1;
                        hdr_count++;
                    }
                }
                if (!next) break;
                line = next + 1;
            }
            free(c);
        }
        closedir(d);
    }
    scan_dir_hdrs(dirpath);
    for (int i = 0; i < hdr_count; i++) {
        if (hdrs[i].count > 5) {
            static char err[256];
            snprintf(err, sizeof(err), "header '%s' included %d times (max 5)", hdrs[i].name, hdrs[i].count);
            return err;
        }
    }
    return NULL;
}
