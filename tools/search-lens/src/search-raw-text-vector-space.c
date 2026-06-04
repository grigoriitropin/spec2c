// SPDX-License-Identifier: Apache-2.0
// search-raw — grep-like search over filesystem, JSON output
// Usage: scan-filesystem /path | search-raw --pattern "malloc" [--type c,h,ipm]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <cjson/cJSON.h>
#include "ipm_time.h"

static int match_ext(const char *path, const char *exts) {
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    dot++; /* skip the dot */
    char buf[256]; strncpy(buf, exts, 255);
    char *tok = strtok(buf, ",");
    while (tok) {
        if (!strcmp(dot, tok)) return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

int main(int argc, char **argv) {
    ipm_time_init();
    const char *pattern = NULL, *exts = "c,h,ipm,md,nix,py,sh";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--pattern") && i+1 < argc) pattern = argv[++i];
        else if (!strcmp(argv[i], "--type") && i+1 < argc) exts = argv[++i];
        else if (argv[i][0] != '-') pattern = argv[i];
    }
    if (!pattern) { fprintf(stderr, "Usage: search-raw --pattern <regex>\n"); return 1; }

    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB)) {
        fprintf(stderr, "search-raw: invalid regex: %s\n", pattern);
        return 1;
    }

    cJSON *results = cJSON_CreateArray();
    int total_matches = 0, files_searched = 0;

    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        cJSON *entry = cJSON_Parse(line);
        if (!entry) continue;

        const char *path = cJSON_GetObjectItemCaseSensitive(entry, "path")->valuestring;
        const char *type = cJSON_GetObjectItemCaseSensitive(entry, "type")->valuestring;
        if (!path || strcmp(type, "file")) { cJSON_Delete(entry); continue; }
        if (!match_ext(path, exts)) { cJSON_Delete(entry); continue; }

        FILE *f = fopen(path, "r");
        if (!f) { cJSON_Delete(entry); continue; }

        files_searched++;
        char file_line[16384]; int lineno = 0;
        while (fgets(file_line, sizeof(file_line), f)) {
            lineno++;
            if (regexec(&regex, file_line, 0, NULL, 0) == 0) {
                total_matches++;
                cJSON *match = cJSON_CreateObject();
                cJSON_AddStringToObject(match, "file", path);
                cJSON_AddNumberToObject(match, "line", lineno);
                /* strip trailing newline */
                size_t l = strlen(file_line);
                while (l && file_line[l-1] == '\n') file_line[--l] = 0;
                cJSON_AddStringToObject(match, "text", file_line);
                cJSON_AddItemToArray(results, match);
            }
        }
        fclose(f);
        cJSON_Delete(entry);
    }
    regfree(&regex);

    cJSON *report = cJSON_CreateObject();
    cJSON_AddItemToObject(report, "matches", results);
    cJSON_AddNumberToObject(report, "total_matches", total_matches);
    cJSON_AddNumberToObject(report, "files_searched", files_searched);
    cJSON_AddStringToObject(report, "pattern", pattern);
    cJSON_AddNumberToObject(report, "elapsed_us", (double)ipm_time_us());

    char *out = cJSON_Print(report);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(report);
    return 0;
}
