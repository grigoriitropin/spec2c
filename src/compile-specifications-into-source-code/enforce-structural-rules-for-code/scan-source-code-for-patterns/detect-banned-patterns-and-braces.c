// SPDX-License-Identifier: Apache-2.0
// shared pattern scanning helpers for enforcement

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
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
    if (strstr(line, "strstr(fns[i].file")) return 0;
    if (strstr(line, "enforce-naming-whitelist-and-validation")) return 0;
    const char *p = line;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/') { p++; continue; } /* skip "//" */
        if (*p >= 'a' && *p <= 'z') return 1;
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

/* ── cross-reference: .ipm file name validation ───────────────────── */
#include <cjson/cJSON.h>

static void validate_single_ipm_name_value(const char *name, const char *what, const char *path) {
    if (!name || !name[0] || !strcmp(name, "main")) return;
    char sep[2] = {strchr(name, '-') ? '-' : '_', 0};
    int w = 0; char buf[256], *tok;
    snprintf(buf, sizeof(buf), "%s", name);
    for (tok = strtok(buf, sep); tok; tok = strtok(NULL, sep)) w++;
    if (w != 5) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §10: .ipm %s '%s' in %s has %d words (need 5)", what, name, path, w);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
}

void verify_ipm_names_across_sources(const char *srcdir) {
    void scan_ipm(const char *dpath) {
        DIR *dd = opendir(dpath); if (!dd) return;
        struct dirent *de2;
        while ((de2 = readdir(dd)) != NULL) {
            if (de2->d_name[0] == '.') continue;
            char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
            struct stat sst; if (stat(sp, &sst) != 0) continue;
            if (S_ISDIR(sst.st_mode)) { scan_ipm(sp); continue; }
            size_t nl = strlen(de2->d_name);
            if (nl <= 5 || strcmp(de2->d_name + nl - 4, ".ipm")) continue;
            FILE *f = fopen(sp, "r"); if (!f) continue;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz <= 0 || sz > 65536) { fclose(f); continue; }
            char *txt = malloc(sz + 1);
            if (!txt) { fclose(f); continue; }
            (void)!fread(txt, 1, sz, f); fclose(f); txt[sz] = 0;
            cJSON *root = cJSON_Parse(txt); free(txt);
            if (!root) continue;
            cJSON *pkg = cJSON_GetObjectItemCaseSensitive(root, "package_name");
            if (pkg && cJSON_IsString(pkg)) validate_single_ipm_name_value(pkg->valuestring, "package_name", sp);
            cJSON *mod = cJSON_GetObjectItemCaseSensitive(root, "module_name");
            if (mod && cJSON_IsString(mod)) validate_single_ipm_name_value(mod->valuestring, "module_name", sp);
            cJSON *funcs = cJSON_GetObjectItemCaseSensitive(root, "function_definitions");
            if (funcs && cJSON_IsObject(funcs)) {
                cJSON *fn = funcs->child;
                while (fn) { validate_single_ipm_name_value(fn->string, "function", sp); fn = fn->next; }
            }
            cJSON_Delete(root);
        }
        closedir(dd);
    }
    scan_ipm(srcdir);
}
