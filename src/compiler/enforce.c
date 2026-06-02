// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
// Checks: file count per dir, line count per file, include frequency
#include "enforce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_FILES_PER_DIR 3
#define MAX_LINES_PER_FILE 400
#define MAX_INCLUDES 2

static void die(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
}

static int count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int lines = 0, ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') lines++;
    fclose(f);
    return lines;
}

static int is_c_or_h(const char *name) {
    size_t nl = strlen(name);
    return nl > 2 && (!strcmp(name + nl - 2, ".c") || !strcmp(name + nl - 2, ".h"));
}

void enforce_structural_limits(const char *srcdir) {
    DIR *d = opendir(srcdir);
    if (!d) die("cannot open source directory");

    struct { char name[64]; int count; } incs[128];
    int inc_qty = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        /* Count .c/.h in this subdirectory */
        int file_cnt = 0;
        DIR *sd = opendir(sub);
        if (sd) {
            struct dirent *se;
            while ((se = readdir(sd)) != NULL) {
                if (!is_c_or_h(se->d_name)) continue;
                file_cnt++;

                /* Line count check */
                if (!strcmp(se->d_name + strlen(se->d_name) - 2, ".c")) {
                    char fp[8192];
                    snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);
                    int lines = count_lines(fp);
                    if (lines > MAX_LINES_PER_FILE) {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d). Split into smaller files.",
                                 fp, lines, MAX_LINES_PER_FILE);
                        die(buf);
                    }
                }

                /* Include frequency check */
                char fp[8192];
                snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);
                FILE *f = fopen(fp, "r");
                if (f) {
                    char line[512];
                    while (fgets(line, sizeof(line), f)) {
                        char hdr[64] = {0};
                        if (sscanf(line, " #include <%63[^>]>", hdr) == 1 ||
                            sscanf(line, " #include \"%63[^\"]\"", hdr) == 1) {
                            int found = 0;
                            for (int i = 0; i < inc_qty; i++) {
                                if (!strcmp(incs[i].name, hdr)) {
                                    if (++incs[i].count > MAX_INCLUDES) {
                                        char buf[512];
                                        snprintf(buf, sizeof(buf),
                                                 "SOUL §7: header '%s' included %d times (max %d). Consolidate into a single shared prelude.",
                                                 hdr, incs[i].count, MAX_INCLUDES);
                                        die(buf);
                                    }
                                    found = 1; break;
                                }
                            }
                            if (!found && inc_qty < 128) {
                                snprintf(incs[inc_qty].name, sizeof(incs[inc_qty].name), "%s", hdr);
                                incs[inc_qty].count = 1;
                                inc_qty++;
                            }
                        }
                    }
                    fclose(f);
                }
            }
            closedir(sd);
        }

        /* File count per directory check */
        if (file_cnt > MAX_FILES_PER_DIR) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d). Split into subdirectories.",
                     sub, file_cnt, MAX_FILES_PER_DIR);
            die(buf);
        }
    }
    closedir(d);
}
