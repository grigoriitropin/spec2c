// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
// Checks: file count per dir, line count per file, include frequency,
//         functions per file, lines per function, banned keywords,
//         hardcoded paths, dead code

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
    return strrchr(line, '{') != NULL;
}

static int has_banned(const char *line) {
    return strstr(line, "goto ") || strstr(line, "goto\t") ||
           strstr(line, "setjmp(") || strstr(line, "longjmp(") ||
           strstr(line, "\"/dev/") || strstr(line, "\"/proc/") || strstr(line, "\"/sys/");
}

static int has_hardcoded_path(const char *line) {
    if (strstr(line, "#include")) return 0;
    const char *p = line;
    while ((p = strstr(p, "\"/")) != NULL) {
        p += 2;
        if (*p == '/' || (*p >= 'a' && *p <= 'z')) return 1;
    }
    return 0;
}

static void extract_func_name(const char *line, char *out, size_t sz) {
    const char *lp = strrchr(line, '(');
    if (!lp) { out[0] = 0; return; }
    while (lp > line && (*(lp-1) == ' ' || *(lp-1) == '\t' || *(lp-1) == '*')) lp--;
    const char *start = lp;
    while (start > line && *(start-1) != ' ' && *(start-1) != '\t' && *(start-1) != '*') start--;
    size_t len = (size_t)(lp - start);
    if (len >= sz) len = sz - 1;
    memcpy(out, start, len);
    out[len] = 0;
}

void enforce_structural_limits(const char *srcdir) {
    DIR *d = opendir(srcdir);
    if (!d) die("cannot open source directory");

    struct { char name[64]; int count; } incs[128];
    int inc_qty = 0;

    /* Dead code tracking: collect all function names */
    struct { char name[128]; char file[256]; } fns[512];
    int fn_qty = 0;

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
                        snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d).",
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
                                    snprintf(buf, sizeof(buf), "SOUL §7: %s has %d functions (max %d).",
                                             fp, func_count, MAX_FUNCTIONS_PER_FILE);
                                    fclose(f); die(buf);
                                }
                                if (fn_qty < 512) {
                                    extract_func_name(line, fns[fn_qty].name, 128);
                                    snprintf(fns[fn_qty].file, 256, "%s", fp);
                                    fn_qty++;
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
                                    snprintf(buf, sizeof(buf), "SOUL §7: %s function #%d is %d lines (max %d).",
                                             fp, func_count, func_lines, MAX_LINES_PER_FUNCTION);
                                    fclose(f); die(buf);
                                }
                                in_func = 0;
                            }
                        }
                        if (is_c && has_banned(line)) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), "SOUL §7: %s uses banned keyword. Remove.",
                                     fp);
                            fclose(f); die(buf);
                        }
                        if (is_c && has_hardcoded_path(line)) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), "SOUL §7: %s has hardcoded absolute path. Use runtime resolution.",
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
                                                 "SOUL §7: header '%s' included %d times (max %d).",
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
            snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d).",
                     sub, file_cnt, MAX_FILES_PER_DIR);
            die(buf);
        }
    }
    closedir(d);

    /* Dead code check: verify every function is called somewhere */
    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        d = opendir(srcdir);
        if (!d) continue;
        while ((de = readdir(d)) != NULL && !called) {
            if (de->d_name[0] == '.') continue;
            char sub[4096];
            snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
            struct stat st2;
            if (stat(sub, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
            DIR *sd2 = opendir(sub);
            if (!sd2) continue;
            struct dirent *se2;
            while ((se2 = readdir(sd2)) != NULL && !called) {
                if (!is_c_or_h(se2->d_name)) continue;
                char fp2[8192];
                snprintf(fp2, sizeof(fp2), "%s/%s", sub, se2->d_name);
                FILE *f3 = fopen(fp2, "r");
                if (!f3) continue;
                char line[1024];
                while (fgets(line, sizeof(line), f3) && !called) {
                    char call[256];
                    snprintf(call, sizeof(call), "%s(", fns[i].name);
                    if (strstr(line, call)) called = 1;
                }
                fclose(f3);
            }
            closedir(sd2);
        }
        closedir(d);
        if (!called) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s is never called.",
                     fns[i].name, fns[i].file);
            die(buf);
        }
    }
}
