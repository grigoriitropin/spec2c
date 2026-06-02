#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static int match_source_code_header_filename(const char *name);
static void report_fatal_error_and_exit(const char *msg);

void enforce_source_code_quality_rules(const char *srcdir, void *fns_data, int fn_qty) {
    struct { char name[128]; char file[256]; } *fns = fns_data;
    struct dirent *de;
    DIR *d;
            char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max %d)", sub, file_cnt, MAX_FILES_PER_DIR);
            report_fatal_error_and_exit(buf);
        }
    }
    closedir(d);

    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        d = opendir(srcdir);
        while ((de = readdir(d)) != NULL && !called) {
        if (de->d_name[0] == '.') continue;
        if (!strcmp(de->d_name, "test") || !strcmp(de->d_name, "tests")) continue;
            char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
            struct stat st2; if (stat(sub, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
            DIR *sd2 = opendir(sub); if (!sd2) continue;
            struct dirent *se2;
            while ((se2 = readdir(sd2)) != NULL && !called) {
                if (!match_source_code_header_filename(se2->d_name)) continue;
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
        if (!called) { char buf[512]; snprintf(buf, sizeof(buf), "SOUL §7: dead code — '%s' in %s is never called", fns[i].name, fns[i].file); report_fatal_error_and_exit(buf); }
    }

    /* Verify exactly one main() in the entry-point file */
    {
        int main_count = 0;
        for (int i = 0; i < fn_qty; i++)
            if (!strcmp(fns[i].name, "main")) main_count++;
        if (main_count != 1) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SOUL §7: exactly one main() required, found %d.", main_count);
            report_fatal_error_and_exit(buf);
        }
    }

    /* Verify all CLI flags are documented in help text (src/ + tools/) */
    for (int dc = 0; dc < 2; dc++) {
        char cdir[4096];
        if (dc == 0) snprintf(cdir, sizeof(cdir), "%s", srcdir);
        else snprintf(cdir, sizeof(cdir), "%s/../tools", srcdir);
        if (access(cdir, F_OK) != 0) continue;
        d = opendir(cdir);
        if (!d) continue;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", cdir, de->d_name);
            struct stat st2; if (stat(sub, &st2) != 0 || !S_ISDIR(st2.st_mode)) continue;
            DIR *sd2 = opendir(sub); if (!sd2) continue;
            struct dirent *se2;
            while ((se2 = readdir(sd2)) != NULL) {
                if (!match_source_code_header_filename(se2->d_name)) continue;
                char fp2[8192]; snprintf(fp2, sizeof(fp2), "%s/%s", sub, se2->d_name);
                FILE *f4 = fopen(fp2, "r"); if (!f4) continue;
                char *content = malloc(65536); if (!content) { fclose(f4); continue; }
                size_t cs = fread(content, 1, 65535, f4); fclose(f4);
                if (cs < 100) { free(content); continue; }
                content[cs] = 0;
                const char *p = content;
                while ((p = strstr(p, "\"--")) != NULL) {
                    p += 2; char flag[64]; int fi = 0;
                    while (*p && *p != '"' && fi < 63) flag[fi++] = *p++;
                    flag[fi] = 0; if (fi == 0) continue;
                    if (!strstr(content, flag)) {
                        char buf[512];
                        snprintf(buf, sizeof(buf), "SOUL §7: flag '%s' in %s not documented in help text.", flag, fp2);
                        free(content); report_fatal_error_and_exit(buf);
                    }
                    /* Check description length: find the help line for this flag */
                    const char *hp = strstr(content, flag);
                    if (hp) {
                        const char *ls = hp;
                        while (ls > content && *(ls-1) != '\n') ls--;
                        const char *le = strchr(hp, '\n');
                        if (!le) le = hp + strlen(hp);
                        int dlen = (int)(le - ls);
                        if (dlen < 40) {
                            char buf[512];
                            snprintf(buf, sizeof(buf), "SOUL §7: flag '%s' help line in %s is %d chars (min 40). Write a descriptive help text.",
                                     flag, fp2, dlen);
                            free(content); report_fatal_error_and_exit(buf);
                        }
                    }
                }
                free(content);
            }
            closedir(sd2);
        }
        closedir(d);
    }
}

void display_current_source_structure_report(const char *srcdir) {
    DIR *d = opendir(srcdir);
    if (!d) { fprintf(stderr, "cannot open %s\n", srcdir); return; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[4096]; snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
        struct stat st; if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        DIR *sd = opendir(sub); if (!sd) continue;
