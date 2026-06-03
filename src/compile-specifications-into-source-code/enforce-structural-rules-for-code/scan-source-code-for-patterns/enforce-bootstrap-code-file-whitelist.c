// SPDX-License-Identifier: Apache-2.0
// bootstrap C whitelist + freeze limits enforcement
#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static char whitelist_names[64][128];
static int whitelist_count_now;

typedef struct { char name[128]; int max_lines; int max_funcs; } freeze_t;
static freeze_t freeze_limits[64];
static int freeze_count;

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

    /* load freeze limits */
    snprintf(path, sizeof(path), "%s/bootstrap-c-freeze-limits.txt", srcdir);
    f = fopen(path, "r");
    if (!f) return; /* optional — limits not enforced if missing */
    char name[128]; int ml, mf;
    while (fgets(line, sizeof(line), f) && freeze_count < 64) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (l == 0 || line[0] == '#') continue;
        if (sscanf(line, "%127s %d %d", name, &ml, &mf) == 3) {
            snprintf(freeze_limits[freeze_count].name, 128, "%s", name);
            freeze_limits[freeze_count].max_lines = ml;
            freeze_limits[freeze_count].max_funcs = mf;
            freeze_count++;
        }
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

void enforce_bootstrap_freeze_limits(const char *srcdir) {
    void walk(const char *dpath) {
        for (int i = 0; i < freeze_count; i++) {
            /* find the file */
            char full[4096];
            snprintf(full, sizeof(full), "%s", srcdir);
            /* find the file by walking entire source tree */
            DIR *dd = opendir(dpath);
            if (!dd) return;
            struct dirent *de;
            while ((de = readdir(dd)) != NULL) {
                if (de->d_name[0] == '.') continue;
                char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dpath, de->d_name);
                struct stat st;
                if (stat(sub, &st) != 0) continue;
                if (S_ISDIR(st.st_mode)) { walk(sub); continue; }
                if (!strcmp(de->d_name, freeze_limits[i].name)) {
                    FILE *f2 = fopen(sub, "r");
                    if (!f2) return;
                    int lines = 0, ch;
                    while ((ch = fgetc(f2)) != EOF) if (ch == '\n') lines++;
                    fclose(f2);
                    if (lines > freeze_limits[i].max_lines) {
                        fprintf(stderr, "spec2c: SOUL §7: bootstrap file %s grew from %d to %d lines — C bootstrap is frozen\n"
                            "  → rewrite the added functionality as an IPM module\n",
                            freeze_limits[i].name, freeze_limits[i].max_lines, lines);
                        exit(1);
                    }
                    return; /* found */
                }
            }
            closedir(dd);
        }
    }
    walk(srcdir);
}
