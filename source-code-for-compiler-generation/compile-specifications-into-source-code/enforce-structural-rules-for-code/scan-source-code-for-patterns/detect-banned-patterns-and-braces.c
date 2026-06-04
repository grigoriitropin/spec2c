// SPDX-License-Identifier: Apache-2.0
// shared pattern scanning helpers for enforcement
#include "../verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void clear_brace_tracking_for_function(brace_state_t *state) {
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
    const char *lp = strchr(line, '(');
    if (!lp) { out[0] = 0; return; }
    while (lp > line && (*(lp-1) == ' ' || *(lp-1) == '\t' || *(lp-1) == '*' || *(lp-1) == '!' || *(lp-1) == '(')) lp--;
    const char *start = lp;
    while (start > line && *(start-1) != ' ' && *(start-1) != '\t' && *(start-1) != '*' && *(start-1) != '!' && *(start-1) != '(' && *(start-1) != ')') start--;
    size_t len = (size_t)(lp - start);
    if (len >= sz) len = sz - 1;
    if (len == 0) { out[0] = 0; return; }
    memcpy(out, start, len); out[len] = 0;
}
static int is_ident_char(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

int check_for_banned_keyword_pattern(const char *line) {
    for (int i = 0; i < banned_patterns_count; i++) {
        const char *pat = banned_patterns[i];
        size_t plen = strlen(pat);
        /* Trim trailing space/tab from pattern for token-boundary matching */
        while (plen > 0 && (pat[plen-1] == ' ' || pat[plen-1] == '\t')) plen--;
        if (plen == 0) continue;
        const char *p = line;
        while ((p = strstr(p, pat))) {
            /* Check left boundary: char before match must not be ident */
            if (p > line && is_ident_char(*(p-1))) { p += plen; continue; }
            /* Check right boundary: char after match must not be ident */
            if (is_ident_char(p[plen])) { p += plen; continue; }
            return 1;
        }
    }
    return 0;
}
int detect_hardcoded_file_path_string(const char *line, const char *filepath) {
    if (strstr(line, "#include")) return 0;
    if (strstr(line, "strstr(fns[i].file")) return 0;
    {
        static const char *exempt_file_suffixes[] = {
            "enforce-naming-whitelist-and-validation",
            NULL
        };
        if (filepath)
            for (int e = 0; exempt_file_suffixes[e]; e++)
                if (strstr(filepath, exempt_file_suffixes[e])) return 0;
    }
    const char *p = line;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/') { p++; continue; }
        if (*p >= 'a' && *p <= 'z') return 1;
    }
    return 0;
}
