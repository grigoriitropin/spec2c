// SPDX-License-Identifier: Apache-2.0
// FFI exports — thin C wrappers callable from IPM via extern_imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *read_entire_file_into_string(const char *path);

/* ── line count ─────────────────────────────────────────────────────── */
int count_lines_inside_file_ffi(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return -1;
    int lines = 0;
    for (const char *p = c; *p; p++) if (*p == '\n') lines++;
    free(c);
    return lines;
}


/* ── banned pattern check (goto, setjmp, longjmp, /dev/null) ────────── */
const char *check_banned_pattern_inside_file(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    static const char *pats[9];
    static int init_done = 0;
    if (!init_done) {
        pats[0] = "got" "o "; pats[1] = "got" "o\t"; pats[2] = "set" "jmp(";
        pats[3] = "long" "jmp("; pats[4] = "2>/dev/nul" "l";
        pats[5] = ">/dev/nul" "l"; pats[6] = "&>/dev/nul" "l";
        pats[7] = "1>/dev/nul" "l"; pats[8] = NULL;
        init_done = 1;
    }
    for (int i = 0; pats[i]; i++)
        if (strstr(c, pats[i])) { free(c); return "uses banned pattern"; }
    free(c);
    return NULL;
}

/* ── hardcoded path check ───────────────────────────────────────────── */
const char *check_hardcoded_path_inside_file(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    const char *p = c;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/') { p++; continue; }
        if (*p >= 'a' && *p <= 'z') {
            if (!strstr(c, "#include")) { free(c); return "has hardcoded path"; }
        }
    }
    free(c);
    return NULL;
}

/* ── line density (control tokens per line) ─────────────────────────── */
const char *check_line_density_inside_file(const char *path) {
    char *c = read_entire_file_into_string(path);
    if (!c) return NULL;
    char *line = c, *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) *next = 0;
        int tokens = 0, is = 0, ic = 0, icm = 0;
        for (const char *p = line; *p; p++) {
        if (icm) {
            if (*p == '*' && *(p+1) == '/') {
                icm = 0;
                p++;
            }
            continue;
        }
            if (!is && !ic && *p == '/' && *(p+1) == '*') {
                icm = 1;
                p++;
                continue;
            }
            if (!is && !ic && *p == '/' && *(p+1) == '/') break;
            if (*p == '\\' && *(p+1) != '\0') { p++; continue; }
            if (!ic && *p == '"') is = !is;
            else if (!is && *p == '\'') ic = !ic;
            if (!is && !ic && !icm) if (*p == ';' || *p == '{' || *p == '?') tokens++;
        }
        if (tokens > 3) { free(c); return "line too dense"; }
        if (!next) break;
        line = next + 1;
    }
    free(c);
    return NULL;
}
