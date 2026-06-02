// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static int debug_trace = 0;

#define MAX_FILES_PER_DIR 3
#define MAX_LINES_PER_FILE 400
#define MAX_INCLUDES 5
#define MAX_FUNCTIONS_PER_FILE 10
#define MAX_LINES_PER_FUNCTION 50

/* ── centralized error reporting ───────────────────────────────────── */
typedef enum {
    ERR_FILE_TOO_LONG,
    ERR_TOO_MANY_FUNCTIONS,
    ERR_FUNCTION_TOO_LONG,
    ERR_TOO_MANY_FILES_IN_DIR,
    ERR_BANNED_PATTERN,
    ERR_HARDCODED_PATH,
    ERR_HEADER_NOT_IN_WHITELIST,
    ERR_HEADER_INCLUDED_TOO_OFTEN,
    ERR_DEAD_CODE,
    ERR_MAIN_COUNT,
    ERR_FLAG_NOT_IN_HELP
} enforce_err_t;

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

static void report_violation_with_actionable_hint(enforce_err_t code, const char *a1,
    int v1, int v2, const char *a2)
{
    char buf[8448];
    switch (code) {
    case ERR_FILE_TOO_LONG:
        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d)\n  → split this file into smaller files", a1, v1, v2); break;
    case ERR_TOO_MANY_FUNCTIONS:
        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d functions (max %d)\n  → extract functions into a new .c file", a1, v1, v2); break;
    case ERR_FUNCTION_TOO_LONG:
        snprintf(buf, sizeof(buf), "SOUL §7: %s func#%d is %d lines (max %d)\n  → split into smaller helper functions", a1, v1, v2, MAX_LINES_PER_FUNCTION); break;
    case ERR_TOO_MANY_FILES_IN_DIR:
        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d files (max %d)\n  → create a subdirectory and move files there", a1, v1, v2); break;
    case ERR_BANNED_PATTERN:
        snprintf(buf, sizeof(buf), "SOUL §7: %s uses banned pattern\n  → remove goto/setjmp/longjmp/output-suppression", a1); break;
    case ERR_HARDCODED_PATH:
        snprintf(buf, sizeof(buf), "SOUL §7: %s has hardcoded path\n  → resolve paths at runtime, never hardcode", a1); break;
    case ERR_HEADER_NOT_IN_WHITELIST:
        snprintf(buf, sizeof(buf), "SOUL §7: header '%s' in %s not in whitelist\n  → add to ok[] in enforce-naming-whitelist-and-validation.c", a1, a2); break;
    case ERR_HEADER_INCLUDED_TOO_OFTEN:
        snprintf(buf, sizeof(buf), "SOUL §7: header '%s' included %d times (max %d)\n  → consolidate includes", a1, v1, v2); break;
    case ERR_DEAD_CODE:
        snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s never called\n  → remove unused function or add call site", a1, a2); break;
    case ERR_MAIN_COUNT:
        snprintf(buf, sizeof(buf), "SOUL §7: exactly one main() required, found %d\n  → keep exactly one entry point", v1); break;
    case ERR_FLAG_NOT_IN_HELP:
        snprintf(buf, sizeof(buf), "SOUL §7: CLI flag '%s' in %s not documented in help text\n  → add flag description to the --help output block", a1, a2); break;
    }
    report_fatal_error_and_exit(buf);
}
static int count_lines_within_source_file(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    int lines = 0, ch; while ((ch = fgetc(f)) != EOF) if (ch == '\n') lines++;
    fclose(f); return lines;
}
static int match_source_code_header_filename(const char *name) {
    size_t nl = strlen(name);
    return nl > 2 && (!strcmp(name + nl - 2, ".c") || !strcmp(name + nl - 2, ".h"));
}
static int detect_function_definition_start_line(const char *line) {
    const char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '/' || *s == '*' || *s == '#' || *s == '\0' || *s == '\n' || *s == '!') return 0;
    if (*s >= '0' && *s <= '9') return 0;
    if (strstr(s, "typedef") || strstr(s, "struct") || strstr(s, "enum")) return 0;
    const char *op = strchr(s, '(');
    const char *ob = strrchr(s, '{');
    if (!op || !ob || op > ob) return 0;
    const char *cp = strchr(op, ')');
    if (!cp || cp > ob) return 0;
    char first[64] = {0};
    const char *id_end = op;
    while (id_end > s && (*(id_end-1) == ' ' || *(id_end-1) == '\t' || *(id_end-1) == '*')) id_end--;
    const char *id_start = id_end;
    while (id_start > s && *(id_start-1) != ' ' && *(id_start-1) != '\t' && *(id_start-1) != '*') id_start--;
    size_t id_len = (size_t)(id_end - id_start);
    if (id_len >= 63) id_len = 63;
    memcpy(first, id_start, id_len);
    if (!strcmp(first, "if") || !strcmp(first, "for") || !strcmp(first, "while") ||
        !strcmp(first, "switch") || !strcmp(first, "return")) return 0;
    if (match_name_against_stdlib_list(first)) return 0;
    return 1;
}

/* ── shared state types ────────────────────────────────────────────── */
typedef struct { char name[128]; char file[256]; } fn_entry_t;
typedef struct { char name[64]; int count; } inc_entry_t;

static void check_include_headers_for_file(const char *sub, inc_entry_t *incs, int *inc_qty);

static void check_single_file_for_violations(const char *sub, int is_c,
    fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty);
static void scan_source_for_undocumented_flags(const char *srcdir);

void enforce_all_source_code_rules(const char *srcdir) {
    read_allowed_names_from_file(srcdir);
    read_banned_patterns_from_file(srcdir);

    fn_entry_t fns[512]; int fn_qty = 0;
    inc_entry_t incs[128]; int inc_qty = 0;

    void scan_dir(const char *dirpath) {
        DIR *d = opendir(dirpath); if (!d) return;
        int file_cnt = 0;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dirpath, de->d_name);
            struct stat st;
            if (stat(sub, &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) {
                validate_name_against_soul_rules("directory", de->d_name, sub);
                scan_dir(sub); continue;
            }
            if (!match_source_code_header_filename(de->d_name)) continue;
            file_cnt++;
            char fname[256]; snprintf(fname, sizeof(fname), "%s", de->d_name);
            char *dot = strrchr(fname, '.'); if (dot) *dot = 0;
            validate_name_against_soul_rules("file", fname, sub);
            int is_c = !strcmp(de->d_name + strlen(de->d_name) - 2, ".c");
            check_single_file_for_violations(sub, is_c, fns, &fn_qty, incs, &inc_qty);
        }
        closedir(d);
        if (file_cnt > MAX_FILES_PER_DIR) {
            report_violation_with_actionable_hint(ERR_TOO_MANY_FILES_IN_DIR, dirpath, file_cnt, MAX_FILES_PER_DIR, NULL);
        }
    }
    scan_dir(srcdir);
    search_for_unused_function_code(fns, fn_qty, srcdir);
    int main_count = 0;
    for (int i = 0; i < fn_qty; i++)
        if (!strcmp(fns[i].name, "main") &&
            !strstr(fns[i].file, "/enforce-structural-rules-for-code/") &&
            !strstr(fns[i].file, "/verify-source-against-soul-patterns/") &&
            !strstr(fns[i].file, "/helper-standalone-executables-for-spec2c/"))
            main_count++;
    if (main_count != 1) {
        report_violation_with_actionable_hint(ERR_MAIN_COUNT, NULL, main_count, 0, NULL);
    }

    scan_source_for_undocumented_flags(srcdir);
}

static void scan_source_for_undocumented_flags(const char *srcdir) {
    void check_flags(const char *dpath) {
        DIR *dd = opendir(dpath); if (!dd) return;
        struct dirent *de2;
        while ((de2 = readdir(dd)) != NULL) {
            if (de2->d_name[0] == '.') continue;
            char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
            struct stat sst;
            if (stat(sp, &sst) != 0) continue;
            if (S_ISDIR(sst.st_mode)) { check_flags(sp); continue; }
            if (strcmp(de2->d_name + strlen(de2->d_name) - 2, ".c")) continue;
            FILE *f4 = fopen(sp, "r"); if (!f4) continue;
            char *content = malloc(65536); if (!content) { fclose(f4); continue; }
            size_t cs = fread(content, 1, 65535, f4); fclose(f4);
            if (cs < 10 || cs >= 65535) { free(content); continue; }
            content[cs] = 0;
            const char *p = content;
            while ((p = strstr(p, "\"--")) != NULL) {
                p += 2;
                char flag[64]; int fi = 0;
                while (*p && *p != '"' && fi < 63) flag[fi++] = *p++;
                flag[fi] = 0;
                if (fi == 0) continue;
                if (!strstr(content, flag)) {
                    free(content); report_violation_with_actionable_hint(ERR_FLAG_NOT_IN_HELP, flag, 0, 0, sp);
                }
            }
            free(content);
        }
        closedir(dd);
    }
    check_flags(srcdir);
}

void display_current_source_structure_report(const char *srcdir) {
    DIR *d = opendir(srcdir);
    if (!d) { fprintf(stderr, "cannot open %s\n", srcdir); return; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
        struct stat st; if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { display_current_source_structure_report(sub); continue; }
        if (!match_source_code_header_filename(de->d_name)) continue;
        int total_lines = count_lines_within_source_file(sub);
        printf("%s: %d lines\n", sub, total_lines);
        FILE *f = fopen(sub, "r");
        if (f) {
            char line[1024];
            int in_func = 0, func_lines = 0;
            brace_state_t bstate; clear_brace_tracking_for_function(&bstate);
            char func_name[128] = {0};
            while (fgets(line, sizeof(line), f)) {
                if (!in_func) {
                    if (detect_function_definition_start_line(line)) {
                        pull_function_name_from_definition(line, func_name, 128);
                        if (!func_name[0]) continue;
                        in_func = 1; func_lines = 1;
                        clear_brace_tracking_for_function(&bstate); count_open_close_brace_pairs(line, &bstate);
                        if (bstate.depth <= 0) { printf("  func: %s (%dL)\n", func_name, func_lines); in_func = 0; }
                        continue;
                    }
                }
                if (in_func) {
                    func_lines++;
                    count_open_close_brace_pairs(line, &bstate);
                    if (bstate.depth <= 0) { printf("  func: %s (%dL)\n", func_name, func_lines); in_func = 0; }
                }
            }
            fclose(f);
        }
    }
    closedir(d);
}

int main(int argc, char **argv) {
    const char *src_dir = "./src";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--debug"))
            debug_trace = 1;
        else
            src_dir = argv[i];
    }
    enforce_all_source_code_rules(src_dir);
    return 0;
}
