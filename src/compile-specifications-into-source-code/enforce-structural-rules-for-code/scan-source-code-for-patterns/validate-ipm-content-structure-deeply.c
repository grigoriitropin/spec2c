// SPDX-License-Identifier: Apache-2.0
// IPM content validators — called from verify_ipm_names_across_sources

#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

void check_ipm_ast_depth_limits(cJSON *node, int depth, const char *path) {
    if (!node || depth > 20) {
        if (depth > 20) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: AST depth > 20 in %s\n  → extract nested logic", path);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
        return;
    }
    if (cJSON_IsObject(node)) {
        int keys = 0;
        cJSON *child = node->child;
        while (child) { keys++; child = child->next; }
        if (keys > 7) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: object has %d keys (max 7) in %s\n  → split into smaller objects", keys, path);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
        child = node->child;
        while (child) { check_ipm_ast_depth_limits(child, depth + 1, path); child = child->next; }
    } else if (cJSON_IsArray(node)) {
        cJSON *child = node->child;
        while (child) {
            if (cJSON_IsArray(child)) {
                char msg[8448]; snprintf(msg, sizeof(msg),
                    "SOUL §7: nested array in %s\n  → use flat list of objects", path);
                fprintf(stderr, "spec2c: %s\n", msg); exit(1);
            }
            check_ipm_ast_depth_limits(child, depth + 1, path); child = child->next;
        }
    }
}

void check_ipm_imports_against_whitelist(cJSON *root, const char *sp,
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

void check_ipm_cli_flag_docs(cJSON *root, const char *sp) {
    (void)sp;
    cJSON *cli = cJSON_GetObjectItemCaseSensitive(root, "cli_flags");
    if (!cli || !cJSON_IsArray(cli)) return;
    cJSON *help = cJSON_GetObjectItemCaseSensitive(root, "help_text");
    for (int fi = 0; fi < cJSON_GetArraySize(cli); fi++) {
        cJSON *flag = cJSON_GetArrayItem(cli, fi);
        if (!cJSON_IsString(flag)) continue;
        if (!help || !cJSON_IsObject(help) ||
            !cJSON_GetObjectItemCaseSensitive(help, flag->valuestring)) {
            char msg[8448]; snprintf(msg, sizeof(msg),
                "SOUL §7: CLI flag '%s' not in help_text\n  → add help_text entry", flag->valuestring);
            fprintf(stderr, "spec2c: %s\n", msg); exit(1);
        }
    }
}

void load_ipm_import_whitelist_file(const char *srcdir,
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
