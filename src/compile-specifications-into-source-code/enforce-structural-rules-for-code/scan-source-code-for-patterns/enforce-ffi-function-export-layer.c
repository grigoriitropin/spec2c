// enforce-ffi-exports.c — thin FFI wrappers for IPM enforcer
// Each function returns non-zero/error-string on violation, 0/NULL on pass
#include "runtime-for-generated-ipm-code.h"
#include <string.h>

/* ── Naming ─────────────────────────────────────────────────────────── */

/* check_name_following_soul_rules — already in check-naming-rules-for-ffi.c */

/* ── Structural ─────────────────────────────────────────────────────── */

int count_lines_inside_file_ffi(const char *path) {
    char *content = read_entire_file_into_string(path);
    if (!content) return -1;
    int lines = 0;
    for (const char *p = content; *p; p++)
        if (*p == '\n') lines++;
    free(content);
    return lines;
}

/* ── Bootstrap whitelist ────────────────────────────────────────────── */
int check_bootstrap_whitelist_for_ffi(const char *basename) {
    extern int match_name_against_bootstrap_list(const char *basename);
    return match_name_against_bootstrap_list(basename);
}

/* ── Pattern matching ───────────────────────────────────────────────── */

/* match_pattern_against_text_string — already in runtime */
/* returns 1 if pattern matches, 0 otherwise */

/* ── File content checkers ──────────────────────────────────────────── */

const char *check_file_content_for_violations(const char *path) {
    char *content = read_entire_file_into_string(path);
    if (!content) return "cannot read file";

    /* check banned patterns (goto, setjmp, etc.) */
    extern int check_for_banned_keyword_pattern(const char *line);
    extern int detect_hardcoded_file_path_string(const char *line);
    extern int banned_patterns_count;
    extern char banned_patterns[32][64];

    /* split into lines and check each */
    char *line = content;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        if (check_for_banned_keyword_pattern(line)) {
            free(content);
            return "banned pattern found (goto/setjmp/longjmp)";
        }
        if (detect_hardcoded_file_path_string(line)) {
            free(content);
            return "hardcoded path found";
        }
        if (!next) break;
        line = next + 1;
    }
    free(content);
    return NULL;
}

/* ── Density (control tokens per line) ──────────────────────────────── */

const char *verify_file_line_density_count(const char *path) {
    char *content = read_entire_file_into_string(path);
    if (!content) return "cannot read file";
    char *line = content;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        int tokens = 0, in_str = 0, in_char = 0, in_comment = 0;
        for (const char *p = line; *p; p++) {
        if (in_comment) {
            if (*p == '*' && *(p+1) == '/') {
                in_comment = 0;
                p++;
            }
            continue;
        }
            if (!in_str && !in_char && *p == '/' && *(p+1) == '*') { in_comment = 1; p++; continue; }
            if (!in_str && !in_char && *p == '/' && *(p+1) == '/') break;
            if (*p == '\\' && *(p+1) != '\0') { p++; continue; }
            if (!in_char && *p == '"') in_str = !in_str;
            else if (!in_str && *p == '\'') in_char = !in_char;
            if (!in_str && !in_char && !in_comment)
                if (*p == ';' || *p == '{' || *p == '?') tokens++;
        }
        if (tokens > 3) { free(content); return "line too dense (>3 control tokens)"; }
        if (!next) break;
        line = next + 1;
    }
    free(content);
    return NULL;
}
