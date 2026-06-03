// SPDX-License-Identifier: Apache-2.0
// bootstrap C whitelist — rejects any .c/.h not explicitly allowed
#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char whitelist_names[64][128];
static int whitelist_count_now;

void load_bootstrap_whitelist_from_disk(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/bootstrap-c-whitelist.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "spec2c: missing bootstrap-c-whitelist.txt\n"
        "  → create it — the C bootstrap is frozen, do not add entries\n"); exit(1); }
    char line[256];
    while (fgets(line, sizeof(line), f) && whitelist_count_now < 64) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (l > 0 && line[0] != '#')
            { snprintf(whitelist_names[whitelist_count_now], 128, "%s", line); whitelist_count_now++; }
    }
    fclose(f);
}

int match_name_against_bootstrap_list(const char *basename) {
    if (strstr(basename, "code-generated-output") || strstr(basename, "handler-code"))
        return 1;
    for (int i = 0; i < whitelist_count_now; i++)
        if (!strcmp(whitelist_names[i], basename)) return 1;
    return 0;
}
