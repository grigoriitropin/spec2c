// SPDX-License-Identifier: Apache-2.0
// shared pattern scanning helpers for enforcement

#include "verify-structural-source-code-rules.h"
#include "soul-naming-forbidden-words-list.h"
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
        if (*p == '/') { p++; continue; }
        if (*p >= 'a' && *p <= 'z') return 1;
    }
    return 0;
}



/* ── cross-reference: .ipm validation ──────────────────────────────── */
#include <cjson/cJSON.h>

static void validate_single_ipm_name_value(const char *name, const char *what, const char *path) {
    if (!name || !name[0] || !strcmp(name, "main")) return;
    char sep[2] = {strchr(name, '-') ? '-' : '_', 0};
    int w = 0; char buf[256], *tok;
    snprintf(buf, sizeof(buf), "%s", name);
    for (tok = strtok(buf, sep); tok; tok = strtok(NULL, sep)) {
        w++;
        if ((int)strlen(tok) < 3) {
            char msg[1024]; snprintf(msg, sizeof(msg),
                "SOUL §10: .ipm %s '%s' in %s: word '%s' too short (min 3)\n"
                "  → rename '%s' to a full English word (≥3 characters)", what, name, path, tok, tok);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
        for (int i = 0; soul_banned_words[i]; i++)
            if (!strcmp(tok, soul_banned_words[i])) {
                char msg[1024]; snprintf(msg, sizeof(msg),
                    "SOUL §10: .ipm %s '%s' in %s: '%s' is a banned type word\n"
                    "  → replace '%s' with a word describing WHAT it does, not WHAT it is",
                    what, name, path, tok, tok);
                fprintf(stderr, "spec2c: %s\n", msg); exit(1);
            }
    }
    if (w != 5) {
        char msg[1024]; snprintf(msg, sizeof(msg),
            "SOUL §10: .ipm %s '%s' in %s has %d words (need 5)\n"
            "  → rename to exactly 5 %s-separated words describing WHAT it does",
            what, name, path, w, sep);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
}

/* scan ALL string values in JSON tree for banned words */
static void scan_json_for_banned_words(cJSON *node, const char *path) {
    if (!node) return;
    if (cJSON_IsString(node)) {
        const char *val = node->valuestring;
        if (!val || !val[0]) return;
        char buf[256]; snprintf(buf, sizeof(buf), "%s", val);
        for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
            for (int i = 0; soul_banned_words[i]; i++)
                if (!strcmp(tok, soul_banned_words[i])) {
                    char msg[512]; snprintf(msg, sizeof(msg),
                        "SOUL §10: .ipm string '%s' in %s contains banned word '%s'\n"
                        "  → replace '%s' with a word describing WHAT it does, not WHAT it is",
                        val, path, tok, tok);
                    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
                }
        }
        return;
    }
    if (cJSON_IsArray(node)) {
        cJSON *child = node->child;
        while (child) { scan_json_for_banned_words(child, path); child = child->next; }
        return;
    }
    if (cJSON_IsObject(node)) {
        cJSON *child = node->child;
        while (child) { scan_json_for_banned_words(child, path); child = child->next; }
    }
}

static void check_ipm_imports_against_whitelist(cJSON *root, const char *sp,
    char allowed[64][128], int n_allowed)
{
    (void)sp;
    cJSON *imps = cJSON_GetObjectItemCaseSensitive(root, "imports");
    if (!imps || !cJSON_IsArray(imps)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(imps); ii++) {
        cJSON *imp = cJSON_GetArrayItem(imps, ii);
        if (!cJSON_IsString(imp)) continue;
        int found = 0;
        for (int wi = 0; wi < n_allowed; wi++)
            if (!strcmp(imp->valuestring, allowed[wi])) { found = 1; break; }
        if (!found) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: import '%s' not in whitelist\n  → add to ipm-imports.whitelist", imp->valuestring);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
    }
}

static void check_ipm_cli_flag_docs(cJSON *root, const char *sp) {
    cJSON *cli = cJSON_GetObjectItemCaseSensitive(root, "cli_flags");
    if (!cli || !cJSON_IsArray(cli)) return;
    cJSON *help = cJSON_GetObjectItemCaseSensitive(root, "help_text");
    if (!help || !cJSON_IsObject(help)) { char msg[8448]; snprintf(msg, sizeof(msg),
        "SOUL §7: %s has cli_flags but no help_text\n  → add a help_text object with flag descriptions", sp);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
    for (int fi = 0; fi < cJSON_GetArraySize(cli); fi++) {
        cJSON *flag = cJSON_GetArrayItem(cli, fi);
        if (!cJSON_IsString(flag)) continue;
        cJSON *entry = cJSON_GetObjectItemCaseSensitive(help, flag->valuestring);
        if (!entry) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: CLI flag '%s' not in help_text\n"
                "  → add \"%s\": \"<description>\" to help_text",
                flag->valuestring, flag->valuestring);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
        if (!cJSON_IsString(entry) || !entry->valuestring || !entry->valuestring[0]) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: CLI flag '%s' has empty help_text\n"
                "  → provide a meaningful description (1+ words) for what the flag does",
                flag->valuestring);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
    }
}

static void load_ipm_import_whitelist_file(const char *srcdir,
    char buf[64][128], int *count)
{
    char wpath[4096]; snprintf(wpath, sizeof(wpath), "%s/ipm-imports.whitelist", srcdir);
    FILE *wf = fopen(wpath, "r");
    if (!wf) return;
    char wline[128];
    while (fgets(wline, sizeof(wline), wf) && *count < 64) {
        size_t wl = strlen(wline);
        while (wl > 0 && (wline[wl-1] == '\n' || wline[wl-1] == '\r')) wline[--wl] = 0;
        if (wl > 0) { snprintf(buf[*count], 128, "%s", wline); (*count)++; }
    }
    fclose(wf);
}

static void check_ipm_ast_depth_limits(cJSON *node, int depth, const char *path) {
    if (!node || depth > 20) {
        if (depth > 20) { char msg[8448]; snprintf(msg, sizeof(msg),
            "SOUL §7: AST depth > 20 in %s\n  → extract nested logic", path);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
        return;
    }
    if (cJSON_IsObject(node)) {
        int keys = 0; cJSON *c = node->child;
        while (c) { keys++; c = c->next; }
        if (keys > 7) { char msg[8448]; snprintf(msg, sizeof(msg),
            "SOUL §7: object has %d keys (max 7) in %s\n  → split into smaller objects", keys, path);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
        c = node->child;
        while (c) { check_ipm_ast_depth_limits(c, depth + 1, path); c = c->next; }
    } else if (cJSON_IsArray(node)) {
        cJSON *c = node->child;
        while (c) {
            if (cJSON_IsArray(c)) { char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: nested array in %s\n  → use flat list of objects", path);
                fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
            check_ipm_ast_depth_limits(c, depth + 1, path); c = c->next;
        }
    }
}

static void validate_single_ipm_file_content(const char *sp,
    char allowed_imports[64][128], int n_allowed)
{
    FILE *f = fopen(sp, "r"); if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) { fclose(f); return; }
    char *txt = malloc(sz + 1);
    if (!txt) { fclose(f); return; }
    (void)!fread(txt, 1, sz, f); fclose(f); txt[sz] = 0;
    cJSON *root = cJSON_Parse(txt); free(txt);
    if (!root) return;

    scan_json_for_banned_words(root, sp);
    check_ipm_ast_depth_limits(root, 0, sp);

    cJSON *pkg = cJSON_GetObjectItemCaseSensitive(root, "package_name");
    if (pkg && cJSON_IsString(pkg)) validate_single_ipm_name_value(pkg->valuestring, "package_name", sp);
    cJSON *mod = cJSON_GetObjectItemCaseSensitive(root, "module_name");
    if (mod && cJSON_IsString(mod)) validate_single_ipm_name_value(mod->valuestring, "module_name", sp);
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(root, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        cJSON *fn = funcs->child;
        while (fn) { validate_single_ipm_name_value(fn->string, "function", sp); fn = fn->next; }
    }

    check_ipm_imports_against_whitelist(root, sp, allowed_imports, n_allowed);
    check_ipm_cli_flag_docs(root, sp);

    /* require non-empty description for all IPM files */
    {   cJSON *desc = cJSON_GetObjectItemCaseSensitive(root, "description");
        if (!desc || !cJSON_IsString(desc) || !desc->valuestring || !desc->valuestring[0]) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: .ipm %s has no description\n"
                "  → add a \"description\" field explaining WHAT this module does", sp);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
    }

    /* dead code + entry point: skip for modules with imports */
    {   cJSON *imps = cJSON_GetObjectItemCaseSensitive(root, "imports");
        int has_imports = imps && cJSON_IsArray(imps) && cJSON_GetArraySize(imps) > 0;
        if (funcs && cJSON_IsObject(funcs) && !has_imports) {
            int main_cnt = 0; cJSON *fn = funcs->child;
            while (fn) { if (!strcmp(fn->string, "main")) main_cnt++; fn = fn->next; }
            if (main_cnt != 1) { char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: .ipm %s has %d main() (need exactly 1)\n  → name exactly one function 'main'", sp, main_cnt);
                fprintf(stderr, "spec2c: %s\n", msg); exit(1); }

            /* collect invocation names via iterative tree walk */
            char called[256][128]; int n_called = 0;
            cJSON *stack[512]; int sp_top = 0;
            fn = funcs->child;
            while (fn) { cJSON *b = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
                if (b) { stack[sp_top++] = b; }
                fn = fn->next; }
            while (sp_top > 0) {
                cJSON *cur = stack[--sp_top];
                if (!cur) continue;
                if (cJSON_IsObject(cur)) {
                    cJSON *inv = cJSON_GetObjectItemCaseSensitive(cur, "invocation_name");
                    if (inv && cJSON_IsString(inv) && n_called < 256) {
                        int dup = 0;
                        for (int ci = 0; ci < n_called; ci++)
                            if (!strcmp(called[ci], inv->valuestring)) { dup = 1; break; }
                        if (!dup) { snprintf(called[n_called],128,"%s",inv->valuestring); n_called++; }
                    }
                    cJSON *c = cur->child;
                    while (c) { stack[sp_top++] = c; c = c->next; }
                } else if (cJSON_IsArray(cur)) {
                    cJSON *c = cur->child;
                    while (c) { stack[sp_top++] = c; c = c->next; }
                }
            }
            fn = funcs->child;
            while (fn) {
                if (strcmp(fn->string, "main")) {
                    int found = 0;
                    for (int ci = 0; ci < n_called; ci++)
                        if (!strcmp(called[ci], fn->string)) { found = 1; break; }
                    if (!found) { char msg[8448]; snprintf(msg, sizeof(msg),
                        "SOUL §7: .ipm dead code — '%s' in %s never called\n  → remove or add invocation", fn->string, sp);
                        fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
                }
                fn = fn->next;
            }
        }
    }

    cJSON_Delete(root);
}

void verify_ipm_names_across_sources(const char *srcdir) {
    char allowed_imports[64][128]; int n_allowed = 0;
    load_ipm_import_whitelist_file(srcdir, allowed_imports, &n_allowed);

    void scan_ipm(const char *dpath) {
        DIR *dd = opendir(dpath); if (!dd) return;
        int ipm_cnt = 0;
        struct dirent *de2;
        while ((de2 = readdir(dd)) != NULL) {
            if (de2->d_name[0] == '.') continue;
            char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
            struct stat sst; if (stat(sp, &sst) != 0) continue;
            if (S_ISDIR(sst.st_mode)) { scan_ipm(sp); continue; }
            size_t nl = strlen(de2->d_name);
            if (nl <= 5 || strcmp(de2->d_name + nl - 4, ".ipm")) continue;
            ipm_cnt++;
            char fname[256]; snprintf(fname, sizeof(fname), "%s", de2->d_name);
            fname[nl - 4] = 0;
            validate_single_ipm_name_value(fname, "file", sp);
            validate_single_ipm_file_content(sp, allowed_imports, n_allowed);
        }
        closedir(dd);
        if (ipm_cnt > 3) {
            char msg[512]; snprintf(msg, sizeof(msg),
                "SOUL §7: %s has %d .ipm files (max 3)\n  → create a subdirectory and move .ipm files there", dpath, ipm_cnt);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
    }
    scan_ipm(srcdir);
}
