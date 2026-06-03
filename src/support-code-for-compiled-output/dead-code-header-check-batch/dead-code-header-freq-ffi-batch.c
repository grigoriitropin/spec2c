// SPDX-License-Identifier: Apache-2.0
// Final FFI: dead code + header frequency (minimal, proven logic from C enforcer)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

extern char *read_entire_file_into_string(const char *path);

/* scan all .c/.h files in dir, call cb for each */
static void iterate_source_files_with_callback(const char *dp, void (*cb)(const char *, const char *)) {
    DIR *d = opendir(dp); if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dp, de->d_name);
        struct stat st; if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { iterate_source_files_with_callback(sub, cb); continue; }
        size_t nl = strlen(de->d_name);
        if (nl > 2 && (!strcmp(de->d_name+nl-2,".c") || !strcmp(de->d_name+nl-2,".h")))
            cb(sub, de->d_name);
    }
    closedir(d);
}

/* ── header include frequency ───────────────────────────────────────── */
static char hdr_names[128][128];
static int  hdr_counts[128];
static int  hdr_total;

static void increment_header_count_on_match(const char *path, const char *name) {
    (void)name;
    char *c = read_entire_file_into_string(path); if (!c) return;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n'); if (next) *next = 0;
        char h[128] = {0};
        if (sscanf(line, " #include \"%127[^\"]\"", h) == 1) {
            int found = 0;
            for (int i = 0; i < hdr_total; i++)
                if (!strcmp(hdr_names[i], h)) { hdr_counts[i]++; found = 1; break; }
            if (!found && hdr_total < 128) {
                snprintf(hdr_names[hdr_total], 128, "%s", h);
                hdr_counts[hdr_total++] = 1;
            }
        }
        if (!next) break;
        line = next + 1;
    }
    free(c);
}

const char *check_header_include_frequency_count(const char *dirpath) {
    hdr_total = 0;
    iterate_source_files_with_callback(dirpath, increment_header_count_on_match);
    for (int i = 0; i < hdr_total; i++) {
        if (hdr_counts[i] > 5) {
            static char err[256];
            snprintf(err, sizeof(err), "header '%s' included %d times (max 5)", hdr_names[i], hdr_counts[i]);
            return err;
        }
    }
    return NULL;
}

/* ── dead code ──────────────────────────────────────────────────────── */
static char fn_names[512][128];
static char fn_files[512][256];
static int  fn_total;

static void store_function_definition_at_path(const char *path, const char *name) {
    (void)name;
    char *c = read_entire_file_into_string(path); if (!c) return;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n'); if (next) *next = 0;
        const char *s = line; while (*s == ' ' || *s == '\t') s++;
        if (*s && *s != '/' && *s != '*' && *s != '#' && *s < '0')
            if (!strstr(s,"typedef") && !strstr(s,"struct") && !strstr(s,"enum")) {
                const char *op = strchr(s, '(');
                const char *ob = strrchr(s, '{');
                if (op && ob && op < ob && strchr(op,')') && strchr(op,')') < ob && fn_total < 512) {
                    const char *ns = op; while (ns > s && (*(ns-1)==' '||*(ns-1)=='\t'||*(ns-1)=='*')) ns--;
                    const char *ne = ns; while (ne > s && *(ne-1)!=' '&&*(ne-1)!='\t'&&*(ne-1)!='*') ne--;
                    size_t nl = (size_t)(ns - ne);
                    if (nl > 0 && nl < 127) {
                        memcpy(fn_names[fn_total], ne, nl); fn_names[fn_total][nl] = 0;
                        snprintf(fn_files[fn_total], 256, "%s", path); fn_total++;
                    }
                }
            }
        if (!next) break;
        line = next + 1;
    }
    free(c);
}

const char *check_dead_code_across_files(const char *dirpath) {
    fn_total = 0; iterate_source_files_with_callback(dirpath, store_function_definition_at_path);
    for (int i = 0; i < fn_total; i++) {
        if (!strcmp(fn_names[i], "main")) continue;
        int called = 0;
        void search_calls(const char *dp) {
            DIR *d = opendir(dp); if (!d || called) { if (d) closedir(d); return; }
            struct dirent *de;
            while ((de = readdir(d)) != NULL && !called) {
                if (de->d_name[0] == '.') continue;
                char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dp, de->d_name);
                struct stat st; if (stat(sub, &st) != 0) continue;
                if (S_ISDIR(st.st_mode)) { search_calls(sub); continue; }
                size_t nl = strlen(de->d_name);
                if (nl < 3 || (strcmp(de->d_name+nl-2,".c") && strcmp(de->d_name+nl-2,".h"))) continue;
                char *c = read_entire_file_into_string(sub); if (!c) continue;
                char call[256]; snprintf(call, sizeof(call), "%s(", fn_names[i]);
                if (strstr(c, call)) called = 1;
                free(c);
            }
            closedir(d);
        }
        search_calls(dirpath);
        if (!called) {
            static char err[256];
            snprintf(err, sizeof(err), "dead code: '%s'", fn_names[i]);
            return err;
        }
    }
    return NULL;
}
