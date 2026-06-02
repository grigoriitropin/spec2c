// SPDX-License-Identifier: Apache-2.0
// shared pattern scanning helpers for enforcement

#include "verify-structural-source-code-rules.h"
#include <string.h>

typedef struct {
    int depth;
    int in_str;
    int in_char;
    int in_comment;
} brace_state_t;

void count_open_close_brace_pairs_reset(brace_state_t *state) {
    memset(state, 0, sizeof(*state));
}

void count_open_close_brace_pairs(const char *line, brace_state_t *state) {
    for (const char *p = line; *p; p++) {
        if (state->in_comment) {
            if (*p == '*' && *(p+1) == '/') { state->in_comment = 0; p++; }
            continue;
        }
        if (!state->in_str && !state->in_char && *p == '/' && *(p+1) == '*') {
            state->in_comment = 1; p++; continue;
        }
        if (!state->in_str && !state->in_char && *p == '/' && *(p+1) == '/') break;
        if (*p == '\\' && *(p+1) != '\0') { p++; continue; }
        if (!state->in_char && *p == '"') state->in_str = !state->in_str;
        else if (!state->in_str && *p == '\'') state->in_char = !state->in_char;
        if (!state->in_str && !state->in_char && !state->in_comment) {
            if (*p == '{') state->depth++;
            else if (*p == '}') state->depth--;
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
