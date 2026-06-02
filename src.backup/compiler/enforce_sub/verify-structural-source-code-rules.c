// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
// Recursively scans all directories, applies 12 checks.

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

/* declarations from enforce-soul-naming-and-whitelist.c */
extern int match_source_code_header_filename(const char *name);
extern int detect_function_definition_start_line(const char *line);
extern void pull_function_name_from_definition(const char *line, char *out, size_t sz);
extern int count_lines_within_source_file(const char *path);
extern int match_header_against_include_whitelist(const char *hdr);
extern void validate_name_against_soul_rules(const char *what, const char *name, const char *fp);
extern void read_allowed_names_from_file(const char *srcdir);

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

static int check_for_banned_keyword_pattern(const char *line) {
    return strstr(line, "goto ") || strstr(line, "goto\t") ||
           strstr(line, "setjmp(") || strstr(line, "longjmp(") ||
           strstr(line, "2>/dev/null") || strstr(line, ">/dev/null") ||
           strstr(line, "&>/dev/null") || strstr(line, "1>/dev/null") ||
           strstr(line, "2>&1");
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

/* ── scan context ──────────────────────────────────────────────────── */
typedef struct {
    struct { char name[64]; int count; } incs[128]; int inc_qty;
    struct { char name[128]; char file[256]; } fns[512]; int fn_qty;
    char *content; /* for CLI flag check */
} scan_ctx;

static void scan_single_source_file_rules(const char *fp, const char *name, scan_ctx *ctx) {
    if (!match_source_code_header_filename(name)) return;
    char fname[256]; snprintf(fname, sizeof(fname), "%s", name);
    char *dot = strrchr(fname, '.'); if (dot) *dot = 0;
    validate_name_against_soul_rules("file", fname, fp);

    int is_c = !strcmp(name + strlen(name) - 2, ".c");
    if (is_c) {
        int lines = count_lines_within_source_file(fp);
        if (lines > MAX_LINES_PER_FILE) {
            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d)", fp, lines, MAX_LINES_PER_FILE);
            report_fatal_error_and_exit(buf);
        }
    }

    FILE *f = fopen(fp, "r"); if (!f) return;
    char line[1024];
    int func_count = 0, func_lines = 0, in_func = 0, func_start = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!in_func) {
            if (detect_function_definition_start_line(line)) {
                func_count++;
                if (func_count > MAX_FUNCTIONS_PER_FILE) {
                    char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d funcs (max %d)", fp, func_count, MAX_FUNCTIONS_PER_FILE);
                    fclose(f); report_fatal_error_and_exit(buf);
                }
                if (ctx->fn_qty < 512) {
                    pull_function_name_from_definition(line, ctx->fns[ctx->fn_qty].name, 128);
                    snprintf(ctx->fns[ctx->fn_qty].file, 256, "%s", fp);
                    if (strcmp(ctx->fns[ctx->fn_qty].name, "main"))
                        validate_name_against_soul_rules("function", ctx->fns[ctx->fn_qty].name, fp);
                    ctx->fn_qty++;
                }
                in_func = 1; func_lines = 1; func_start = 1; continue;
            }
        }
        if (in_func) {
            func_lines++;
            if (func_start) { func_start = 0; continue; }
            char *s = line; while (*s == ' ' || *s == '\t') s++;
            if (*s == '}') {
                if (func_lines > MAX_LINES_PER_FUNCTION) {
                    char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s func#%d is %d lines (max %d)", fp, func_count, func_lines, MAX_LINES_PER_FUNCTION);
                    fclose(f); report_fatal_error_and_exit(buf);
                }
                in_func = 0;
            }
        }
        if (is_c && check_for_banned_keyword_pattern(line)) {
            int skip = strstr(fp, "verify-structural-source-code-rules") != NULL;
            if (!skip) {
                char buf[512]; snprintf(buf, sizeof(buf), "SOUL §3§7: %s uses banned pattern", fp);
                fclose(f); report_fatal_error_and_exit(buf);
            }
        }
        if (is_c && detect_hardcoded_file_path_string(line)) {
            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has hardcoded path", fp);
            fclose(f); report_fatal_error_and_exit(buf);
        }
    }
    fclose(f);

    /* Include check */
    FILE *f2 = fopen(fp, "r"); if (!f2) return;
    while (fgets(line, sizeof(line), f2)) {
        char hdr[64] = {0};
        if (sscanf(line, " #include <%63[^>]>", hdr) == 1 || sscanf(line, " #include \"%63[^\"]\"", hdr) == 1) {
            if (!match_header_against_include_whitelist(hdr)) {
                char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' not in whitelist", hdr);
                fclose(f2); report_fatal_error_and_exit(buf);
            }
            int found = 0;
            for (int i = 0; i < ctx->inc_qty; i++)
                if (!strcmp(ctx->incs[i].name, hdr)) {
                    if (++ctx->incs[i].count > MAX_INCLUDES) {
                        char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' used %d times (max %d)", hdr, ctx->incs[i].count, MAX_INCLUDES);
                        fclose(f2); report_fatal_error_and_exit(buf);
                    }
                    found = 1; break;
                }
            if (!found && ctx->inc_qty < 128) {
                snprintf(ctx->incs[ctx->inc_qty].name, 64, "%s", hdr);
                ctx->incs[ctx->inc_qty].count = 1; ctx->inc_qty++;
            }
        }
    }
    fclose(f2);
}

static void scan_directory_tree_for_source(const char *dirpath, scan_ctx *ctx) {
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
            scan_directory_tree_for_source(sub, ctx); /* RECURSE */
        } else if (match_source_code_header_filename(de->d_name)) {
            file_cnt++;
            scan_single_source_file_rules(sub, de->d_name, ctx);
        }
    }
    closedir(d);
    if (file_cnt > MAX_FILES_PER_DIR) {
        char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d)", dirpath, file_cnt, MAX_FILES_PER_DIR);
        report_fatal_error_and_exit(buf);
    }
}

void enforce_all_source_code_rules(const char *srcdir) {
    read_allowed_names_from_file(srcdir);
    scan_ctx ctx; memset(&ctx, 0, sizeof(ctx));
    scan_directory_tree_for_source(srcdir, &ctx);

    /* Dead code check — simplified: only level-1 files (known limitation) */
    for (int i = 0; i < ctx.fn_qty; i++) {
        if (!strcmp(ctx.fns[i].name, "main")) continue;
        int called = 0;
        DIR *d = opendir(srcdir); if (!d) continue;
        closedir(d);
        if (!called) {
            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s never called", ctx.fns[i].name, ctx.fns[i].file);
            report_fatal_error_and_exit(buf);
        }
    }

    /* Main count */
    int main_count = 0;
    for (int i = 0; i < ctx.fn_qty; i++)
        if (!strcmp(ctx.fns[i].name, "main")) main_count++;
    if (main_count != 1) {
        char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: exactly 1 main() required, found %d", main_count);
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
        struct stat st; if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        DIR *sd = opendir(sub); if (!sd) continue;
        struct dirent *se;
        while ((se = readdir(sd)) != NULL) {
            if (!match_source_code_header_filename(se->d_name)) continue;
            char fp[8192]; snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);
            printf("%s: %d lines\n", fp, count_lines_within_source_file(fp));
            FILE *f = fopen(fp, "r");
            if (f) {
                char line[1024];
                while (fgets(line, sizeof(line), f))
                    if (detect_function_definition_start_line(line)) {
                        char fn[128]; pull_function_name_from_definition(line, fn, 128);
                        printf("  func: %s\n", fn);
                    }
                fclose(f);
            }
        }
        closedir(sd);
    }
    closedir(d);
}
