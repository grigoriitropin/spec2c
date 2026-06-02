// SPDX-License-Identifier: Apache-2.0
// shared pattern scanning helpers for enforcement

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

void count_open_close_brace_pairs(const char *line, int *depth) {
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

void pull_function_name_from_definition(const char *line, char *out, size_t sz) {
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

int check_for_banned_keyword_pattern(const char *line) {
    for (int i = 0; i < banned_patterns_count; i++)
        if (strstr(line, banned_patterns[i])) return 1;
    return 0;
}

int detect_hardcoded_file_path_string(const char *line) {
    if (strstr(line, "#include")) return 0;
    const char *p = line;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/' || (*p >= 'a' && *p <= 'z')) return 1;
    }
    return 0;
}
