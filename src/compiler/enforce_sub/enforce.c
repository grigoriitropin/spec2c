// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
// Checks: file count per dir, line count per file, include frequency,
//         functions per file, lines per function, banned keywords

#include "enforce.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_FILES_PER_DIR 3
#define MAX_LINES_PER_FILE 400
#define MAX_INCLUDES 3
#define MAX_FUNCTIONS_PER_FILE 10
#define MAX_LINES_PER_FUNCTION 50

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

static int is_func_start(const char *line) {
    while (*line == ' ' || *line == '\t') return 0;
    if (*line == '/' || *line == '*' || *line == '#' || *line == '\0' || *line == '\n') return 0;
    if (strstr(line, "typedef") || strstr(line, "struct") || strstr(line, "enum")) return 0;
    const char *brace = strrchr(line, '{');
    return brace != NULL;
}

static int has_banned(const char *line) {
    return strstr(line, "goto ") || strstr(line, "goto\t") ||
           strstr(line, "setjmp(") || strstr(line, "longjmp(");
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

        int file_cnt = 0;
        DIR *sd = opendir(sub);
        if (sd) {
            struct dirent *se;
            while ((se = readdir(sd)) != NULL) {
                if (!is_c_or_h(se->d_name)) continue;
                file_cnt++;

                char fp[8192];
                snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);

                int is_c = !strcmp(se->d_name + strlen(se->d_name) - 2, ".c");

                if (is_c) {
                    int lines = count_lines(fp);
                    if (lines > MAX_LINES_PER_FILE) {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d). Split into smaller files.",
                                 fp, lines, MAX_LINES_PER_FILE);
                        die(buf);
                    }
                }

                FILE *f = fopen(fp, "r");
                if (f) {
                    char line[1024];
                    int func_count = 0, func_lines = 0, in_func = 0, func_start = 0;
                    while (fgets(line, sizeof(line), f)) {
                        if (!in_func) {
                            if (is_func_start(line)) {
                                func_count++;
                                if (func_count > MAX_FUNCTIONS_PER_FILE) {
                                    char buf[512];
                                    snprintf(buf, sizeof(buf), "SOUL §7: %s has %d functions (max %d). Extract into separate files.",
                                             fp, func_count, MAX_FUNCTIONS_PER_FILE);
                                    fclose(f); die(buf);
                                }
                                in_func = 1; func_lines = 1; func_start = 1;
                                continue;
                            }
                        }
                        if (in_func) {
                            func_lines++;
                            if (func_start) { func_start = 0; continue; }
                            char *s = line;
                            while (*s == ' ' || *s == '\t') s++;
                            if (*s == '}') {
                                if (func_lines > MAX_LINES_PER_FUNCTION) {
                                    char buf[512];
                                    snprintf(buf, sizeof(buf), "SOUL §7: %s function #%d is %d lines (max %d). Extract or split.",
                                             fp, func_count, func_lines, MAX_LINES_PER_FUNCTION);
                                    fclose(f); die(buf);
                                }
                                in_func = 0;
                            }
                        }
                        if (is_c && has_banned(line)) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), "SOUL §7: %s uses banned keyword (goto/setjmp/longjmp). Remove.",
                                     fp);
                            fclose(f); die(buf);
                        }
                    }
                    fclose(f);
                }

                FILE *f2 = fopen(fp, "r");
                if (f2) {
                    char line[512];
                    while (fgets(line, sizeof(line), f2)) {
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
                                        fclose(f2); die(buf);
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
                    fclose(f2);
                }
            }
            closedir(sd);
        }

        if (file_cnt > MAX_FILES_PER_DIR) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d). Split into subdirectories.",
                     sub, file_cnt, MAX_FILES_PER_DIR);
            die(buf);
        }
    }
    closedir(d);
}
