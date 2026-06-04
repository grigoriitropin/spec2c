// SPDX-License-Identifier: Apache-2.0
// IPM file validation — name/value scanning, dead code, CLI docs, type enforcement
#include "../../verify-structural-source-code-rules.h"
#include "../../../shared-type-declarations-across-modules/soul-naming-forbidden-words-list.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <cjson/cJSON.h>

/* ── cross-reference: .ipm validation ──────────────────────────────── */
#include <cjson/cJSON.h>
static void validate_single_ipm_name_value(const char *name, const char *what, const char *path) {
    if (!name || !name[0]) {
        fprintf(stderr, "spec2c: SOUL §10: empty %s in %s\n", what, path ? path : "?");
        exit(1);
    }
    if (!strcmp(name, "main")) return;
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
static void check_banned_in_ipm_string_value(const char *val, const char *key, const char *path) {
    char *buf = strdup(val);
    if (buf) {
        for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
            for (int i = 0; soul_banned_words[i]; i++)
                if (!strcmp(tok, soul_banned_words[i])) {
                    char msg[512]; snprintf(msg, sizeof(msg),
                        "SOUL §10: .ipm string '%s' in %s contains banned word '%s'\n"
                        "  → replace '%s' with a word describing WHAT it does, not WHAT it is",
                        val, path, tok, tok);
                    free(buf);
                    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
                }
        }
        free(buf);
    }
    if (strstr(val, "malloc(") || strstr(val, "free(") || strstr(val, "sizeof(")) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §7: C-leak — IPM string contains malloc/free/sizeof at %s\n"
            "  → remove malloc, free, sizeof from IPM strings", path);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
    int is_include_ok = !strcmp(key, "template_str") || !strcmp(key, "str");
    if (!is_include_ok && strstr(val, "#include")) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §7: C-leak — #include only allowed in template_str/str at %s\n"
            "  → move #include to template_str or str field", path);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
    if (strstr(val, "goto") || strstr(val, "setjmp") ||
        strstr(val, "longjmp") || strstr(val, "#pragma") ||
        strstr(val, "_Pragma") ||
        strstr(val, "2>/dev/null") || strstr(val, "2>&1")) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §7: banned pattern in IPM string at %s\n"
            "  → remove goto/setjmp/longjmp/#pragma weak from IPM strings", path);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
    if (strstr(val, "\"/")) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §7: hardcoded path in IPM string at %s\n"
            "  → resolve paths at runtime, never hardcode", path);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
}

static void check_banned_in_ipm_json_key(const char *kn, const char *path) {
    if (strstr(kn, "goto") || strstr(kn, "setjmp") ||
        strstr(kn, "longjmp") || strstr(kn, "#pragma") ||
        strstr(kn, "2>") || strstr(kn, "#include") ||
        strstr(kn, "malloc") || strstr(kn, "free(") ||
        strstr(kn, "sizeof")) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "SOUL §7: banned pattern in IPM key '%s' at %s\n"
            "  → rename the key, banned patterns are not allowed in key names", kn, path);
        fprintf(stderr, "spec2c: %s\n", msg); exit(1);
    }
}

static void scan_json_for_banned_words(cJSON *node, const char *path) {
    if (!node) return;
    if (cJSON_IsString(node)) {
        if (node->valuestring && node->valuestring[0])
            check_banned_in_ipm_string_value(node->valuestring,
                node->string ? node->string : "", path);
        return;
    }
    if (cJSON_IsArray(node)) {
        cJSON *child = node->child;
        while (child) { scan_json_for_banned_words(child, path); child = child->next; }
        return;
    }
    if (cJSON_IsObject(node)) {
        cJSON *child = node->child;
        while (child) {
            if (child->string) check_banned_in_ipm_json_key(child->string, path);
            scan_json_for_banned_words(child, path); child = child->next;
        }
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
    FILE *f = fopen(sp, "r"); if (!f) {
        fprintf(stderr, "spec2c: FATAL: cannot open IPM file %s\n", sp); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) { fclose(f);
        fprintf(stderr, "spec2c: FATAL: IPM file %s size %ld invalid\n", sp, sz); exit(1); }
    char *txt = malloc(sz + 1);
    if (!txt) { fclose(f);
        fprintf(stderr, "spec2c: FATAL: malloc %ld for %s failed\n", sz + 1, sp); exit(1); }
    (void)!fread(txt, 1, sz, f); fclose(f); txt[sz] = 0;
    cJSON *root = cJSON_Parse(txt); free(txt);
    if (!root) {
        fprintf(stderr, "spec2c: FATAL: JSON parse error in %s\n", sp); exit(1); }

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

    /* strict type check */
    {   cJSON *funcs2 = cJSON_GetObjectItemCaseSensitive(root, "function_definitions");
        if (funcs2 && cJSON_IsObject(funcs2)) {
            cJSON *fn = funcs2->child;
            while (fn) {
                cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
                if (body && cJSON_IsArray(body)) {
                    for (int bi = 0; bi < cJSON_GetArraySize(body); bi++) {
                        cJSON *inst = cJSON_GetArrayItem(body, bi);
                        if (!inst) continue;
                        cJSON *vt = cJSON_GetObjectItemCaseSensitive(inst, "variable_type");
                        if (vt && cJSON_IsString(vt) && vt->valuestring[0] &&
                            !match_type_against_strict_whitelist(vt->valuestring))
                            { char msg[512]; snprintf(msg, sizeof(msg),
                                "SOUL §7: strict type — '%s' not allowed\n  → use u32,i32,u64,str,cjson,ptr", vt->valuestring);
                              fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
                        cJSON *tt = cJSON_GetObjectItemCaseSensitive(inst, "target_type");
                        if (tt && cJSON_IsString(tt) && tt->valuestring[0]) {
                            /* alu_operation types: only i32, u32, u64 */
                            if (strcmp(tt->valuestring, "i32") && strcmp(tt->valuestring, "u32") &&
                                strcmp(tt->valuestring, "u64"))
                                { char msg[512]; snprintf(msg, sizeof(msg),
                                    "SOUL §7: ALU target_type '%s' not allowed\n  → use u32, i32, u64 for alu_operation", tt->valuestring);
                                  fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
                        }
                        cJSON *pt = cJSON_GetObjectItemCaseSensitive(inst, "parameter_type");
                        if (pt && cJSON_IsString(pt) && pt->valuestring[0] &&
                            !match_type_against_strict_whitelist(pt->valuestring))
                            { char msg[512]; snprintf(msg, sizeof(msg),
                                "SOUL §7: strict type — '%s' not allowed\n  → use u32,i32,u64,str,cjson,ptr", pt->valuestring);
                              fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
                    }
                }
                cJSON *pars = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
                if (pars && cJSON_IsArray(pars)) {
                    for (int pi = 0; pi < cJSON_GetArraySize(pars); pi++) {
                        cJSON *pt = cJSON_GetObjectItemCaseSensitive(
                            cJSON_GetArrayItem(pars, pi), "parameter_type");
                        if (pt && cJSON_IsString(pt) && pt->valuestring[0] &&
                            !match_type_against_strict_whitelist(pt->valuestring))
                            { char msg[512]; snprintf(msg, sizeof(msg),
                                "SOUL §7: strict parameter type — '%s' not allowed\n  → use u32,i32,u64,str,cjson,ptr", pt->valuestring);
                              fprintf(stderr, "spec2c: %s\n", msg); exit(1); }
                    }
                }
                fn = fn->next;
            }
        }
    }

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
    {   char wpath[4096]; snprintf(wpath, sizeof(wpath), "%s/ipm-imports.whitelist", srcdir);
        FILE *wf = fopen(wpath, "r");
        if (wf) {
            char wline[128];
            while (fgets(wline, sizeof(wline), wf) && n_allowed < 64) {
                size_t wl = strlen(wline);
                while (wl > 0 && (wline[wl-1] == '\n' || wline[wl-1] == '\r')) wline[--wl] = 0;
                if (wl > 0) { snprintf(allowed_imports[n_allowed], 128, "%s", wline); n_allowed++; }
            }
            fclose(wf);
        }
    }

    void scan_ipm(const char *dpath) {
        DIR *dd = opendir(dpath); if (!dd) {
            fprintf(stderr, "spec2c: FATAL: cannot open directory %s\n", dpath); exit(1); }
        int ipm_cnt = 0;
        struct dirent *de2;
        while ((de2 = readdir(dd)) != NULL) {
            if (!strcmp(de2->d_name, ".") || !strcmp(de2->d_name, "..")) continue;
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
