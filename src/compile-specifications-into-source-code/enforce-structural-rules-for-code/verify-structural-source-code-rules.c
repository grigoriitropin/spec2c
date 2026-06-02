// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES_PER_DIR 3
#define MAX_LINES_PER_FILE 400
#define MAX_INCLUDES 3
#define MAX_FUNCTIONS_PER_FILE 10
#define MAX_LINES_PER_FUNCTION 50

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
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
    if (!strncmp(first, "cJSON_", 6)) return 0;
    if (!strcmp(first, "strstr") || !strcmp(first, "strncmp") || !strcmp(first, "strcmp") ||
        !strcmp(first, "strlen") || !strcmp(first, "sscanf") || !strcmp(first, "snprintf") ||
        !strcmp(first, "printf") || !strcmp(first, "fprintf") || !strcmp(first, "sprintf") ||
        !strcmp(first, "malloc") || !strcmp(first, "realloc") || !strcmp(first, "free") ||
        !strcmp(first, "fopen") || !strcmp(first, "fclose")) return 0;
    return 1;
}
static int detect_hardcoded_file_path_string(const char *line) {
    if (strstr(line, "#include")) return 0;
    const char *p = line;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/' || (*p >= 'a' && *p <= 'z')) return 1;
    }
    return 0;
}
static void pull_function_name_from_definition(const char *line, char *out, size_t sz) {
    const char *lp = strrchr(line, '(');
    if (!lp) { out[0] = 0; return; }
    while (lp > line && (*(lp-1) == ' ' || *(lp-1) == '\t' || *(lp-1) == '*' || *(lp-1) == '!' || *(lp-1) == '(')) lp--;
    const char *start = lp;
    while (start > line && *(start-1) != ' ' && *(start-1) != '\t' && *(start-1) != '*' && *(start-1) != '!' && *(start-1) != '(' && *(start-1) != ')') start--;
    size_t len = (size_t)(lp - start);
    if (len >= sz) len = sz - 1;
    if (len == 0) { out[0] = 0; return; }
    memcpy(out, start, len); out[len] = 0;
}

/* ── brace counter (string-aware) ─────────────────────────────────── */
static void count_open_close_brace_pairs(const char *line, int *depth) {
    int in_str = 0, in_char = 0;
    for (const char *p = line; *p; p++) {
        if (!in_str && !in_char && *p == '/' && *(p+1) == '/') break;
        if (*p == '\\' && *(p+1) != '\0') { p++; continue; }
        if (!in_char && *p == '"') in_str = !in_str;
        else if (!in_str && *p == '\'') in_char = !in_char;
        if (!in_str && !in_char) {
            if (*p == '{') (*depth)++;
            else if (*p == '}') (*depth)--;
        }
    }
}

/* ── shared state types ────────────────────────────────────────────── */
typedef struct { char name[128]; char file[256]; } fn_entry_t;
typedef struct { char name[64]; int count; } inc_entry_t;

static void check_include_headers_for_file(const char *sub, inc_entry_t *incs, int *inc_qty);

static void check_single_file_for_violations(const char *sub, int is_c,
    fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty)
{
    if (is_c) {
        int lines = count_lines_within_source_file(sub);
        if (lines > MAX_LINES_PER_FILE) {
            char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d)", sub, lines, MAX_LINES_PER_FILE);
            report_fatal_error_and_exit(buf);
        }
    }
    FILE *f = fopen(sub, "r");
    if (f) {
        char line[1024];
        int func_count = 0, func_lines = 0, in_func = 0, func_start = 0, depth = 0;
        while (fgets(line, sizeof(line), f)) {
            if (!in_func) {
                if (detect_function_definition_start_line(line)) {
                    func_count++;
                    if (func_count > MAX_FUNCTIONS_PER_FILE) {
                        char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d functions (max %d)", sub, func_count, MAX_FUNCTIONS_PER_FILE);
                        fclose(f); report_fatal_error_and_exit(buf);
                    }
                    if (*fn_qty < 512) {
                        pull_function_name_from_definition(line, fns[*fn_qty].name, 128);
                        if (fns[*fn_qty].name[0]) {
                            snprintf(fns[*fn_qty].file, 256, "%.255s", sub);
                            if (strcmp(fns[*fn_qty].name, "main"))
                                validate_name_against_soul_rules("function", fns[*fn_qty].name, sub);
                            (*fn_qty)++;
                        }
                    }
                    in_func = 1; func_lines = 1; func_start = 1;
                    depth = 0; count_open_close_brace_pairs(line, &depth);
                    if (depth <= 0) { in_func = 0; }
                    continue;
                }
            }
            if (in_func) {
                func_lines++;
                if (func_start) { func_start = 0; continue; }
                count_open_close_brace_pairs(line, &depth);
                if (depth <= 0) {
                    if (func_lines > MAX_LINES_PER_FUNCTION) {
                        char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s func#%d is %d lines (max %d)", sub, func_count, func_lines, MAX_LINES_PER_FUNCTION);
                        fclose(f); report_fatal_error_and_exit(buf);
                    }
                    in_func = 0; depth = 0;
                }
            }
            if (is_c && check_for_banned_keyword_pattern(line)) {
                char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s uses banned pattern", sub);
                fclose(f); report_fatal_error_and_exit(buf);
            }
            if (is_c && detect_hardcoded_file_path_string(line)) {
                char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s has hardcoded absolute path", sub);
                fclose(f); report_fatal_error_and_exit(buf);
            }
        }
        fclose(f);
    }
    check_include_headers_for_file(sub, incs, inc_qty);
}

static void check_include_headers_for_file(const char *sub, inc_entry_t *incs, int *inc_qty) {
    FILE *f2 = fopen(sub, "r");
    if (f2) {
        char line[512];
        while (fgets(line, sizeof(line), f2)) {
            char hdr[128] = {0}; int is_angle = 0;
            if (sscanf(line, " #include <%127[^>]>", hdr) == 1) is_angle = 1;
            else if (sscanf(line, " #include \"%127[^\"]\"", hdr) == 1) is_angle = 0;
            else continue;
            if (!match_header_against_include_whitelist(hdr)) {
                char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' not in whitelist", hdr);
                fclose(f2); report_fatal_error_and_exit(buf);
            }
            if (!is_angle) {
                int found = 0;
                for (int i = 0; i < *inc_qty; i++)
                    if (!strcmp(incs[i].name, hdr)) {
                        if (++incs[i].count > MAX_INCLUDES) {
                            char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' included %d times (max %d)", hdr, incs[i].count, MAX_INCLUDES);
                            fclose(f2); report_fatal_error_and_exit(buf);
                        }
                        found = 1; break;
                    }
                if (!found && *inc_qty < 128) {
                    snprintf(incs[*inc_qty].name, 64, "%s", hdr);
                    incs[*inc_qty].count = 1; (*inc_qty)++;
                }
            }
        }
        fclose(f2);
    }
}

static void search_for_unused_function_code(fn_entry_t *fns, int fn_qty, const char *srcdir) {
    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        void search_calls(const char *dpath) {
            DIR *dd = opendir(dpath); if (!dd) return;
            struct dirent *de2;
            while ((de2 = readdir(dd)) != NULL && !called) {
                if (de2->d_name[0] == '.') continue;
                char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
                struct stat sst; if (stat(sp, &sst) != 0) continue;
                if (S_ISDIR(sst.st_mode)) { search_calls(sp); continue; }
                if (!match_source_code_header_filename(de2->d_name)) continue;
                FILE *rf = fopen(sp, "r"); if (!rf) continue;
                char rl[1024];
                while (fgets(rl, sizeof(rl), rf) && !called) {
                    char call[256]; snprintf(call, sizeof(call), "%s(", fns[i].name);
                    if (strstr(rl, call)) called = 1;
                }
                fclose(rf);
            }
            closedir(dd);
        }
        search_calls(srcdir);
        if (!called) {
            char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s never called", fns[i].name, fns[i].file);
            report_fatal_error_and_exit(buf);
        }
    }
}

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
            char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d)", dirpath, file_cnt, MAX_FILES_PER_DIR);
            report_fatal_error_and_exit(buf);
        }
    }
    scan_dir(srcdir);
    search_for_unused_function_code(fns, fn_qty, srcdir);
    for (int i = 0; i < fn_qty; i++)
        if (!strcmp(fns[i].name, "main")) main_count++;
    if (main_count != 1) {
        char buf[8448]; snprintf(buf, sizeof(buf), "SOUL §7: exactly one main() required, found %d", main_count);
        report_fatal_error_and_exit(buf);
    }
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
            int in_func = 0, func_lines = 0, func_start = 0, depth = 0;
            char func_name[128] = {0};
            while (fgets(line, sizeof(line), f)) {
                if (!in_func) {
                    if (detect_function_definition_start_line(line)) {
                        pull_function_name_from_definition(line, func_name, 128);
                        if (!func_name[0]) continue;
                        in_func = 1; func_lines = 1; func_start = 1;
                        depth = 0; count_open_close_brace_pairs(line, &depth);
                        if (depth <= 0) { printf("  func: %s (%dL)\n", func_name, func_lines); in_func = 0; depth = 0; }
                        continue;
                    }
                }
                if (in_func) {
                    func_lines++;
                    if (func_start) { func_start = 0; continue; }
                    count_open_close_brace_pairs(line, &depth);
                    if (depth <= 0) { printf("  func: %s (%dL)\n", func_name, func_lines); in_func = 0; depth = 0; }
                }
            }
            fclose(f);
        }
    }
    closedir(d);
}

int main(int argc, char **argv) {
    const char *src_dir = (argc > 1) ? argv[1] : "./src";
    enforce_all_source_code_rules(src_dir);
    return 0;
}
