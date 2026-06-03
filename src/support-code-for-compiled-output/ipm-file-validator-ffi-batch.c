// SPDX-License-Identifier: Apache-2.0
// FFI wrapper: runs all IPM validation from C enforcer via single call
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

extern char *read_entire_file_into_string(const char *path);

/* re-use C enforcer's IPM banned words list */
static const char *banned_words_ffi[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

static int check_token_banned_word_test(const char *w) {
    for (int i = 0; banned_words_ffi[i]; i++)
        if (!strcmp(w, banned_words_ffi[i])) return 1;
    return 0;
}

static int validate_ipm_naming_compliance_check(const char *name, char *err, size_t esz) {
    if (!name || !name[0] || !strcmp(name, "main")) return 1;
    char sep = strchr(name, '-') ? '-' : '_';
    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0;
    char *tok = strtok(buf, &sep); /* &sep is UB — use string instead */
    char sep_str[2] = {sep, 0};
    tok = strtok(buf, sep_str);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            snprintf(err, esz, "word '%s' too short (min 3)", tok); return 0;
        }
        if (check_token_banned_word_test(tok)) {
            snprintf(err, esz, "banned word '%s'", tok); return 0;
        }
        tok = strtok(NULL, sep_str);
    }
    if (words != 5) {
        snprintf(err, esz, "has %d words (need 5)", words); return 0;
    }
    return 1;
}

/* walk JSON tree checking for banned words in ALL string values */
static int banned_word_json_tree_scanner(cJSON *node, char *err, size_t esz) {
    if (!node) return 1;
    if (cJSON_IsString(node)) {
        char buf[256]; snprintf(buf, sizeof(buf), "%s", node->valuestring);
        char *tok = strtok(buf, " ");
        while (tok) {
            if (check_token_banned_word_test(tok)) {
                snprintf(err, esz, "banned word '%s' in string", tok); return 0;
            }
            tok = strtok(NULL, " ");
        }
        return 1;
    }
    if (cJSON_IsArray(node)) {
        cJSON *c = node->child;
        while (c) { if (!banned_word_json_tree_scanner(c, err, esz)) return 0; c = c->next; }
    }
    if (cJSON_IsObject(node)) {
        cJSON *c = node->child;
        while (c) { if (!banned_word_json_tree_scanner(c, err, esz)) return 0; c = c->next; }
    }
    return 1;
}

/* check AST depth and object keys */
static int validate_ipm_structure_limits_check(cJSON *node, int depth, char *err, size_t esz) {
    if (!node || depth > 20) {
        if (depth > 20) { snprintf(err, esz, "AST depth > 20"); return 0; }
        return 1;
    }
    if (cJSON_IsObject(node)) {
        int keys = 0; cJSON *c = node->child;
        while (c) { keys++; c = c->next; }
        if (keys > 7) { snprintf(err, esz, "object has %d keys (max 7)", keys); return 0; }
        c = node->child;
        while (c) { if (!validate_ipm_structure_limits_check(c, depth+1, err, esz)) return 0; c = c->next; }
    } else if (cJSON_IsArray(node)) {
        cJSON *c = node->child;
        while (c) {
            if (cJSON_IsArray(c)) { snprintf(err, esz, "nested array"); return 0; }
            if (!validate_ipm_structure_limits_check(c, depth+1, err, esz)) return 0;
            c = c->next;
        }
    }
    return 1;
}

const char *check_ipm_file_validation_ffi(const char *path) {
    static char err[512];
    char *content = read_entire_file_into_string(path);
    if (!content) return NULL;
    cJSON *root = cJSON_Parse(content);
    free(content);
    if (!root) return "invalid JSON";

    /* check package/module/function names */
    cJSON *pkg = cJSON_GetObjectItemCaseSensitive(root, "package_name");
    if (pkg && cJSON_IsString(pkg) && !validate_ipm_naming_compliance_check(pkg->valuestring, err, sizeof(err)))
        { cJSON_Delete(root); return err; }
    cJSON *mod = cJSON_GetObjectItemCaseSensitive(root, "module_name");
    if (mod && cJSON_IsString(mod) && !validate_ipm_naming_compliance_check(mod->valuestring, err, sizeof(err)))
        { cJSON_Delete(root); return err; }
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(root, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        cJSON *fn = funcs->child;
        while (fn) {
            if (fn->string && !validate_ipm_naming_compliance_check(fn->string, err, sizeof(err)))
                { cJSON_Delete(root); return err; }
            fn = fn->next;
        }
    }

    /* banned words in strings */
    if (!banned_word_json_tree_scanner(root, err, sizeof(err)))
        { cJSON_Delete(root); return err; }

    /* structure checks */
    if (!validate_ipm_structure_limits_check(root, 0, err, sizeof(err)))
        { cJSON_Delete(root); return err; }

    /* description required */
    cJSON *desc = cJSON_GetObjectItemCaseSensitive(root, "description");
    if (!desc || !cJSON_IsString(desc) || !desc->valuestring[0])
        { cJSON_Delete(root); return "missing description"; }

    /* CLI flags check */
    cJSON *cli = cJSON_GetObjectItemCaseSensitive(root, "cli_flags");
    if (cli && cJSON_IsArray(cli)) {
        cJSON *help = cJSON_GetObjectItemCaseSensitive(root, "help_text");
        if (!help || !cJSON_IsObject(help))
            { cJSON_Delete(root); return "cli_flags without help_text"; }
        for (int fi = 0; fi < cJSON_GetArraySize(cli); fi++) {
            cJSON *flag = cJSON_GetArrayItem(cli, fi);
            if (!cJSON_IsString(flag)) continue;
            if (!cJSON_GetObjectItemCaseSensitive(help, flag->valuestring))
                { snprintf(err, sizeof(err), "flag '%s' not in help_text", flag->valuestring);
                  cJSON_Delete(root); return err; }
        }
    }

    /* dead code check (if no imports) */
    cJSON *imps = cJSON_GetObjectItemCaseSensitive(root, "imports");
    int has_imports = imps && cJSON_IsArray(imps) && cJSON_GetArraySize(imps) > 0;
    if (funcs && cJSON_IsObject(funcs) && !has_imports) {
        int main_cnt = 0;
        cJSON *fn = funcs->child;
        while (fn) {
            if (!strcmp(fn->string, "main")) main_cnt++;
            fn = fn->next;
        }
        if (main_cnt != 1)
            { cJSON_Delete(root); return "need exactly 1 main()"; }
    }

    cJSON_Delete(root);
    return NULL;
}
