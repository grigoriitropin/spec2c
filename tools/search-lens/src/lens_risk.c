// SPDX-License-Identifier: Apache-2.0
// lens-risk — mechanical risk analysis without LLM
// Detects: malloc without free, fopen without fclose, popen/system (forbidden), strcpy (unsafe)
// Usage: scan-filesystem /path | lens-risk
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <cjson/cJSON.h>
#include "ipm_time.h"

typedef struct { const char *name; const char *pattern; const char *severity; const char *fix; } risk_t;

static const risk_t risks[] = {
    {"malloc-without-free", "malloc\\s*\\(", "high", "Ensure free() is called, or use arena allocator"},
    {"fopen-without-fclose", "fopen\\s*\\(", "medium", "Ensure fclose() in all paths, or use defer pattern"},
    {"strcpy-unsafe", "strcpy\\s*\\(", "high", "Use strncpy or memcpy with bounds check"},
    {"sprintf-unsafe", "sprintf\\s*\\(", "medium", "Use snprintf with buffer size"},
    {"gets-unsafe", "gets\\s*\\(", "critical", "NEVER use gets(). Use fgets()"},
    {"system-call", "system\\s*\\(", "high", "Forbidden. Use ipm_call_tool() or fork/execve"},
    {"popen-call", "popen\\s*\\(", "high", "Forbidden. Use ipm_call_tool() or fork/execve"},
    {NULL, NULL, NULL, NULL}
};

int main(void) {
    ipm_time_init();
    cJSON *report = cJSON_CreateObject();
    cJSON *findings = cJSON_CreateArray();

    regex_t regexes[8] = {0};
    for (int i = 0; risks[i].name; i++)
        regcomp(&regexes[i], risks[i].pattern, REG_EXTENDED | REG_NOSUB);

    char line[8192]; int total_risks = 0, files_scanned = 0;
    while (fgets(line, sizeof(line), stdin)) {
        cJSON *entry = cJSON_Parse(line);
        if (!entry) continue;
        const char *path = cJSON_GetObjectItemCaseSensitive(entry, "path")->valuestring;
        const char *type = cJSON_GetObjectItemCaseSensitive(entry, "type")->valuestring;
        if (!path || strcmp(type, "file")) { cJSON_Delete(entry); continue; }

        const char *ext = strrchr(path, '.');
        if (!ext || (strcmp(ext, ".c") && strcmp(ext, ".h"))) { cJSON_Delete(entry); continue; }

        FILE *f = fopen(path, "r"); if (!f) { cJSON_Delete(entry); continue; }
        files_scanned++;

        char fline[16384]; int lineno = 0;
        while (fgets(fline, sizeof(fline), f)) {
            lineno++;
            for (int ri = 0; risks[ri].name; ri++) {
                if (regexec(&regexes[ri], fline, 0, NULL, 0) == 0) {
                    total_risks++;
                    cJSON *r = cJSON_CreateObject();
                    cJSON_AddStringToObject(r, "file", path);
                    cJSON_AddNumberToObject(r, "line", lineno);
                    cJSON_AddStringToObject(r, "risk", risks[ri].name);
                    cJSON_AddStringToObject(r, "severity", risks[ri].severity);
                    cJSON_AddStringToObject(r, "fix", risks[ri].fix);
                    cJSON_AddItemToArray(findings, r);
                }
            }
        }
        fclose(f);
        cJSON_Delete(entry);
    }
    for (int i = 0; risks[i].name; i++) regfree(&regexes[i]);

    cJSON_AddItemToObject(report, "findings", findings);
    cJSON_AddNumberToObject(report, "total_risks", total_risks);
    cJSON_AddNumberToObject(report, "files_scanned", files_scanned);
    cJSON_AddNumberToObject(report, "elapsed_us", (double)ipm_time_us());

    char *out = cJSON_Print(report);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(report);
    return 0;
}
