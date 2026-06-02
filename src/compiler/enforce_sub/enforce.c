// SPDX-License-Identifier: Apache-2.0
// enforce.c — structural enforcement for spec2c compiler source
// 9 checks: files/dir, lines/file, includes/freq, funcs/file,
//           lines/func, banned(goto/setjmp/longjmp), hardcoded paths,
//           dead code, naming whitelist
// Naming whitelist loaded from src/allowed-names.txt at startup.

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

static void die(const char *msg) { fprintf(stderr, "spec2c: %s\n", msg); exit(1); }

static int count_lines(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    int lines = 0, ch; while ((ch = fgetc(f)) != EOF) if (ch == '\n') lines++;
    fclose(f); return lines;
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
           strstr(line, "setjmp(") || strstr(line, "longjmp(");
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
    memcpy(out, start, len); out[len] = 0;
}

static int is_allowed_include(const char *hdr) {
    const char *ok[] = {
        "stdio.h","stdlib.h","string.h","errno.h","unistd.h","fcntl.h",
        "sys/stat.h","sys/types.h","sys/wait.h","sys/socket.h","sys/un.h",
        "dirent.h","regex.h","stdint.h","stddef.h","stdbool.h","time.h",
        "cjson/cJSON.h","netinet/in.h",
        "ipm_builtins.h","enforce.h","common_h/common.h","enforce_sub/enforce.h",
        "../common_h/common.h","vehir_lib.h","ipm_json.h","header.h", NULL
    };
    for (int i = 0; ok[i]; i++) if (!strcmp(hdr, ok[i])) return 1;
    return 0;
}

/* ── naming whitelist ──────────────────────────────────────────────── */
static struct { char name[128]; } allowed[512];
static int allowed_qty = 0;

static void load_allowed(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/allowed-names.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) die("cannot open allowed-names.txt");
    char line[256];
    while (fgets(line, sizeof(line), f) && allowed_qty < 512) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0) { snprintf(allowed[allowed_qty].name, 128, "%s", line); allowed_qty++; }
    }
    fclose(f);
}

static int is_allowed(const char *name) {
    for (int i = 0; i < allowed_qty; i++)
        if (!strcmp(allowed[i].name, name)) return 1;
    return 0;
}

static void check_name(const char *what, const char *name, const char *fp) {
    const char *banned_type[] = {
        "service","server","daemon","library","tool","binary",
        "package","module","system","utility","application",
        "program","process","worker",NULL
    };
    const char *soful = "SOUL §10 (immutable): 'Exactly 5 words, hyphen-separated. No more, no less. "
        "No type words. Banned: service,server,daemon,library,tool,binary,package,module,"
        "system,utility,application,program,process,worker. "
        "Describes WHAT it does, not what it is or how it is built. "
        "English only. Full words over abbreviations. "
        "Self-documenting.'";

    char is_file = (what[0] == 'f');
    char sep = is_file ? '-' : '_';

    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0;
    char *tok = strtok(buf, &sep);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char eb[2048];
            snprintf(eb, sizeof(eb),
                "SOUL §10: word '%s' in %s '%s' at %s is too short (min 3 chars).\n%s",
                tok, what, name, fp, soful);
            die(eb);
        }
        for (int i = 0; banned_type[i]; i++)
            if (!strcmp(tok, banned_type[i])) {
                char eb[2048];
                snprintf(eb, sizeof(eb),
                    "SOUL §10: word '%s' in %s '%s' at %s is a banned type word.\n%s",
                    tok, what, name, fp, soful);
                die(eb);
            }
        tok = strtok(NULL, &sep);
    }
    if (words != 5) {
        char eb[2048];
        snprintf(eb, sizeof(eb),
            "SOUL §10: %s '%s' at %s has %d words — exactly 5 required (%s-separated).\n%s",
            what, name, fp, words, is_file ? "hyphen" : "underscore", soful);
        die(eb);
    }
    if (!is_allowed(name)) {
        char eb[2048];
        snprintf(eb, sizeof(eb),
            "SOUL §10: %s '%s' at %s — not in allowed-names.txt.\n%s",
            what, name, fp, soful);
        die(eb);
    }
}

void enforce_structural_limits(const char *srcdir) {
    load_allowed(srcdir);
    DIR *d = opendir(srcdir);
    if (!d) die("cannot open source directory");

    struct { char name[64]; int count; } incs[128]; int inc_qty = 0;
    struct { char name[128]; char file[256]; } fns[512]; int fn_qty = 0;
    struct dirent *de;

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        int file_cnt = 0;
        DIR *sd = opendir(sub);
        if (sd) {
            struct dirent *se;
            while ((se = readdir(sd)) != NULL) {
                if (!is_c_or_h(se->d_name)) continue;
                file_cnt++;
                char fp[8192]; snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);

                char fname[256]; snprintf(fname, sizeof(fname), "%s", se->d_name);
                char *dot = strrchr(fname, '.'); if (dot) *dot = 0;
                check_name("file", fname, fp);

                int is_c = !strcmp(se->d_name + strlen(se->d_name) - 2, ".c");
                if (is_c) {
                    int lines = count_lines(fp);
                    if (lines > MAX_LINES_PER_FILE) {
                        char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d lines (max %d)", fp, lines, MAX_LINES_PER_FILE);
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
                                    char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d functions (max %d)", fp, func_count, MAX_FUNCTIONS_PER_FILE);
                                    fclose(f); die(buf);
                                }
                                if (fn_qty < 512) {
                                    extract_func_name(line, fns[fn_qty].name, 128);
                                    snprintf(fns[fn_qty].file, 256, "%s", fp);
                                    check_name("function", fns[fn_qty].name, fp);
                                    fn_qty++;
                                }
                                in_func = 1; func_lines = 1; func_start = 1;
                                continue;
                            }
                        }
                        if (in_func) {
                            func_lines++;
                            if (func_start) { func_start = 0; continue; }
                            char *s = line; while (*s == ' ' || *s == '\t') s++;
                            if (*s == '}') {
                                if (func_lines > MAX_LINES_PER_FUNCTION) {
                                    char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s func#%d is %d lines (max %d)", fp, func_count, func_lines, MAX_LINES_PER_FUNCTION);
                                    fclose(f); die(buf);
                                }
                                in_func = 0;
                            }
                        }
                        if (is_c && has_banned(line)) {
                            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s uses banned keyword (goto/setjmp/longjmp)", fp);
                            fclose(f); die(buf);
                        }
                        if (is_c && has_hardcoded_path(line)) {
                            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has hardcoded absolute path", fp);
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
                            if (!is_allowed_include(hdr)) {
                                char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' not in whitelist", hdr);
                                fclose(f2); die(buf);
                            }
                            int found = 0;
                            for (int i = 0; i < inc_qty; i++)
                                if (!strcmp(incs[i].name, hdr)) { if (++incs[i].count > MAX_INCLUDES) { char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: header '%s' included %d times (max %d)", hdr, incs[i].count, MAX_INCLUDES); fclose(f2); die(buf); } found = 1; break; }
                            if (!found && inc_qty < 128) { snprintf(incs[inc_qty].name, 64, "%s", hdr); incs[inc_qty].count = 1; inc_qty++; }
                        }
                    }
                    fclose(f2);
                }
            }
            closedir(sd);
        }
        if (file_cnt > MAX_FILES_PER_DIR) {
            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d)", sub, file_cnt, MAX_FILES_PER_DIR);
            die(buf);
        }
    }
    closedir(d);

    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        d = opendir(srcdir);
        while ((de = readdir(d)) != NULL && !called) {
            if (de->d_name[0] == '.') continue;
            char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
            struct stat st2; if (stat(sub, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
            DIR *sd2 = opendir(sub); if (!sd2) continue;
            struct dirent *se2;
            while ((se2 = readdir(sd2)) != NULL && !called) {
                if (!is_c_or_h(se2->d_name)) continue;
                char fp2[8192]; snprintf(fp2, sizeof(fp2), "%s/%s", sub, se2->d_name);
                FILE *f3 = fopen(fp2, "r"); if (!f3) continue;
                char line[1024];
                while (fgets(line, sizeof(line), f3) && !called) {
                    char call[256]; snprintf(call, sizeof(call), "%s(", fns[i].name);
                    if (strstr(line, call)) called = 1;
                }
                fclose(f3);
            }
            closedir(sd2);
        }
        closedir(d);
        if (!called) { char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s is never called", fns[i].name, fns[i].file); die(buf); }
    }
}

void print_source_structure(const char *srcdir) {
    DIR *d = opendir(srcdir);
    if (!d) { fprintf(stderr, "cannot open %s\n", srcdir); return; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
        struct stat st; if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        DIR *sd = opendir(sub); if (!sd) continue;
        struct dirent *se;
        while ((se = readdir(sd)) != NULL) {
            if (!is_c_or_h(se->d_name)) continue;
            char fp[8192]; snprintf(fp, sizeof(fp), "%s/%s", sub, se->d_name);
            printf("%s: %d lines\n", fp, count_lines(fp));
            FILE *f = fopen(fp, "r");
            if (f) {
                char line[1024];
                while (fgets(line, sizeof(line), f))
                    if (is_func_start(line)) { char fn[128]; extract_func_name(line, fn, 128); printf("  func: %s\n", fn); }
                fclose(f);
            }
        }
        closedir(sd);
    }
    closedir(d);
}
