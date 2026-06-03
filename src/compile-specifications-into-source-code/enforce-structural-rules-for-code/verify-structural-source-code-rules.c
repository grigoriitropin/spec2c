// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cjson/cJSON.h>
static int lint_mode = 0;
static int lint_errors = 0;
static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}
void report_violation_with_actionable_hint(enforce_err_t code, const char *a1,
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
    case ERR_LINE_TOO_DENSE:
        snprintf(buf, sizeof(buf), "SOUL §7: %s line %d is too dense — %d control tokens (; { ?) (max 3)\n  → split the line into multiple statements", a1, v1, v2); break;
    case ERR_NOT_IN_BOOTSTRAP_WHITELIST:
        snprintf(buf, sizeof(buf), "SOUL §7: %s not in bootstrap C whitelist\n  → rewrite this functionality as an IPM module, the C bootstrap is frozen", a1); break;
    }
    if (lint_mode) {
        fprintf(stderr, "spec2c: %s\n", buf);
        lint_errors++;
    } else
        report_fatal_error_and_exit(buf);
}

int match_source_code_header_filename(const char *name) {
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
void check_include_headers_for_file(const char *sub, inc_entry_t *incs, int *inc_qty);
static void check_line_density_within_source(const char *line, const char *sub, int file_line) {
    int in_str = 0, in_char = 0, in_comment = 0, tokens = 0;
    for (const char *p = line; *p; p++) {
        if (in_comment) {
            if (*p == '*' && *(p+1) == '/') { in_comment = 0; p++; }
            continue;
        }
        if (!in_str && !in_char && *p == '/' && *(p+1) == '*') {
            in_comment = 1;
            p++;
            continue;
        }
        if (!in_str && !in_char && *p == '/' && *(p+1) == '/') break;
        if (*p == '\\' && *(p+1) != '\0') { p++; continue; }
        if (!in_char && *p == '"') in_str = !in_str;
        else if (!in_str && *p == '\'') in_char = !in_char;
        if (!in_str && !in_char && !in_comment)
            if (*p == ';' || *p == '{' || *p == '?') tokens++;
    }
    if (tokens > 3)
        report_violation_with_actionable_hint(ERR_LINE_TOO_DENSE, sub, file_line, tokens, NULL);
}

static int handle_new_function_definition_entry(const char *line, const char *sub,
    int *func_count, fn_entry_t *fns, int *fn_qty, brace_state_t *bstate) {
    (*func_count)++;
    if (*func_count > MAX_FUNCTIONS_PER_FILE) {
        report_violation_with_actionable_hint(ERR_TOO_MANY_FUNCTIONS, sub, *func_count, MAX_FUNCTIONS_PER_FILE, NULL);
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
    clear_brace_tracking_for_function(bstate);
    count_open_close_brace_pairs(line, bstate);
    return bstate->depth <= 0 ? 0 : 1;
}

static void verify_line_for_banned_hardcoded(const char *line, const char *sub, int is_source) {
    if (is_source && check_for_banned_keyword_pattern(line)) {
        report_violation_with_actionable_hint(ERR_BANNED_PATTERN, sub, 0, 0, NULL);
    }
    if (is_source && detect_hardcoded_file_path_string(line)) {
        report_violation_with_actionable_hint(ERR_HARDCODED_PATH, sub, 0, 0, NULL);
    }
}

void check_single_file_for_violations(const char *sub, int is_c, int is_source,
    fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty)
{
    if (is_c) {
        int lines = count_lines_within_source_file(sub);
        if (lines > MAX_LINES_PER_FILE)
            report_violation_with_actionable_hint(ERR_FILE_TOO_LONG, sub, lines, MAX_LINES_PER_FILE, NULL);
    }
    FILE *f = fopen(sub, "r");
    if (!f) { check_include_headers_for_file(sub, incs, inc_qty); return; }
    char line[1024];
    int func_count = 0, func_lines = 0, in_func = 0, file_line = 0;
    brace_state_t bstate; clear_brace_tracking_for_function(&bstate);
    while (fgets(line, sizeof(line), f)) {
        file_line++;
        check_line_density_within_source(line, sub, file_line);
        if (!in_func) {
            if (detect_function_definition_start_line(line)) {
                in_func = handle_new_function_definition_entry(line, sub, &func_count, fns, fn_qty, &bstate);
                func_lines = 1;
                continue;
            }
        }
        if (in_func) {
            func_lines++;
            count_open_close_brace_pairs(line, &bstate);
            if (bstate.depth <= 0) {
                if (func_lines > MAX_LINES_PER_FUNCTION)
                    report_violation_with_actionable_hint(ERR_FUNCTION_TOO_LONG, sub, func_count, func_lines, NULL);
                in_func = 0;
            }
        }
        verify_line_for_banned_hardcoded(line, sub, is_source);
    }
    fclose(f);
    check_include_headers_for_file(sub, incs, inc_qty);
}
void check_include_headers_for_file(const char *sub, inc_entry_t *incs, int *inc_qty) {
    FILE *f2 = fopen(sub, "r");
    if (f2) {
        char line[512];
        while (fgets(line, sizeof(line), f2)) {
            char hdr[128] = {0}; int is_angle = 0;
            if (sscanf(line, " #include <%127[^>]>", hdr) == 1) is_angle = 1;
            else if (sscanf(line, " #include \"%127[^\"]\"", hdr) == 1) is_angle = 0;
            else continue;
            if (!match_header_against_include_whitelist(hdr)) {
                fclose(f2); report_violation_with_actionable_hint(ERR_HEADER_NOT_IN_WHITELIST, hdr, 0, 0, sub);
            }
            if (!is_angle) {
                int found = 0;
                for (int i = 0; i < *inc_qty; i++)
                    if (!strcmp(incs[i].name, hdr)) {
                        if (++incs[i].count > MAX_INCLUDES) {
                            fclose(f2); report_violation_with_actionable_hint(ERR_HEADER_INCLUDED_TOO_OFTEN, hdr, incs[i].count, MAX_INCLUDES, NULL);
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
            report_violation_with_actionable_hint(ERR_DEAD_CODE, fns[i].name, 0, 0, fns[i].file);
        }
    }
}
static void scan_source_for_undocumented_flags(const char *srcdir);


extern void scan_each_directory_with_checks(const char *dirpath, fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty);

void enforce_all_source_code_rules(const char *srcdir) {
    read_allowed_names_from_file(srcdir);
    read_banned_patterns_from_file(srcdir);
    load_non_source_file_allowlist(srcdir);
    load_bootstrap_whitelist_from_disk(srcdir);
    fn_entry_t fns[512]; int fn_qty = 0;
    inc_entry_t incs[128]; int inc_qty = 0;
    scan_each_directory_with_checks(srcdir, fns, &fn_qty, incs, &inc_qty);
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
    verify_ipm_names_across_sources(srcdir);
    enforce_bootstrap_code_freeze_check(srcdir);
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
            char *content = malloc(65536);
            if (!content) { fclose(f4); continue; }
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

int main(int argc, char **argv) {
    const char *src_dir = "./src";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--lint"))
            lint_mode = 1;
        else
            src_dir = argv[i];
    }
    enforce_all_source_code_rules(src_dir);
    if (lint_mode && lint_errors > 0) {
        fprintf(stderr, "\nspec2c: %d errors found\n", lint_errors);
        return 1;
    }
    return 0;
}
int match_name_against_stdlib_list(const char *name) {
    const char *lib[] = {
        "strstr","strncmp","strcmp","strlen","sscanf","snprintf",
        "printf","fprintf","sprintf","malloc","realloc","free",
        "fopen","fclose","fread","fgets","fputs","fflush",
        "memcpy","memset","strdup","strtok","strrchr","strchr",
        "calloc","exit",NULL
    };
    if (!strncmp(name, "cJSON_", 6)) return 1;
    for (int i = 0; lib[i]; i++)
        if (!strcmp(name, lib[i])) return 1;
    return 0;
}
