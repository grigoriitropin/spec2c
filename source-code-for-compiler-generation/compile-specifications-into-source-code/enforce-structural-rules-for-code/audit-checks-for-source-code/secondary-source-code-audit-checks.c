// SPDX-License-Identifier: Apache-2.0
#include "../verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void check_include_headers_for_file(const char *sub) {
    inc_entry_t incs[128]; int inc_qty = 0;
    FILE *f2 = fopen(sub, "r");
    if (!f2) report_fatal_error_and_exit("cannot open file for include audit");
    char line[512];
    while (fgets(line, sizeof(line), f2)) {
        char hdr[128] = {0}; int is_angle = 0;
        if (sscanf(line, " #include <%127[^>]>", hdr) == 1) is_angle = 1;
        else if (sscanf(line, " #include \"%127[^\"]\"", hdr) == 1) is_angle = 0;
        else if (sscanf(line, " # include <%127[^>]>", hdr) == 1) is_angle = 1;
        else if (sscanf(line, " # include \"%127[^\"]\"", hdr) == 1) is_angle = 0;
        else continue;
        if (!match_header_against_include_whitelist(hdr)) {
            fclose(f2); report_violation_with_actionable_hint(ERR_HEADER_NOT_IN_WHITELIST, hdr, 0, 0, sub);
        }
        if (!is_angle) {
            int found = 0;
            for (int i = 0; i < inc_qty; i++)
                if (!strcmp(incs[i].name, hdr)) {
                    if (++incs[i].count > MAX_INCLUDES) {
                        fclose(f2); report_violation_with_actionable_hint(ERR_HEADER_INCLUDED_TOO_OFTEN, hdr, incs[i].count, MAX_INCLUDES, NULL);
                    }
                    found = 1; break;
                }
            if (!found && inc_qty < 128) {
                snprintf(incs[inc_qty].name, 64, "%s", hdr);
                incs[inc_qty].count = 1; inc_qty++;
            }
        }
    }
    fclose(f2);
}

void search_for_unused_function_code(fn_entry_t *fns, int fn_qty, const char *srcdir) {
    for (int i = 0; i < fn_qty; i++) {
        if (!strcmp(fns[i].name, "main")) continue;
        int called = 0;
        void search_calls(const char *dpath) {
            DIR *dd = opendir(dpath); if (!dd) report_fatal_error_and_exit("cannot open dir in dead-code search");
            struct dirent *de2;
            while ((de2 = readdir(dd)) != NULL && !called) {
                if (de2->d_name[0] == '.') continue;
                char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
                struct stat sst; if (stat(sp, &sst) != 0) continue;
                if (S_ISDIR(sst.st_mode)) { search_calls(sp); continue; }
                FILE *rf = fopen(sp, "r"); if (!rf) report_fatal_error_and_exit("cannot open source file in dead-code search");
                char rl[1024];
                while (fgets(rl, sizeof(rl), rf) && !called) {
                    char call[256]; snprintf(call, sizeof(call), "%s(", fns[i].name);
                    if (strstr(rl, call)) called = 1;
                }
                fclose(rf);
            }
            closedir(dd);
        }
        search_calls(srcdir);
        if (!called) {
            report_violation_with_actionable_hint(ERR_DEAD_CODE, fns[i].name, 0, 0, fns[i].file);
        }
    }
}

void skip_root_files_when_scanning(const char *srcdir)
{
    if (!strcmp(srcdir, ".")) {
        DIR *d = opendir(srcdir);
        if (!d) report_fatal_error_and_exit("cannot open root directory for scan");
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            char sub[8192];
            snprintf(sub, sizeof(sub), "%s/%s", srcdir, de->d_name);
            struct stat st;
            if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode))
                verify_ipm_names_across_sources(sub);
        }
        closedir(d);
    } else {
        verify_ipm_names_across_sources(srcdir);
    }
}

int check_main_count_exemption_rule(const char *path) {
    for (int i = 0; main_exempt_directory_names[i]; i++) {
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "/%s/", main_exempt_directory_names[i]);
        if (strstr(path, pattern)) return 1;
    }
    return 0;
}

void scan_source_for_undocumented_flags(const char *srcdir) {
    void check_flags(const char *dpath) {
        DIR *dd = opendir(dpath); if (!dd) report_fatal_error_and_exit("cannot open dir for flag scan");
        struct dirent *de2;
        while ((de2 = readdir(dd)) != NULL) {
            if (de2->d_name[0] == '.') continue;
            char sp[8192]; snprintf(sp, sizeof(sp), "%s/%s", dpath, de2->d_name);
            struct stat sst;
            if (stat(sp, &sst) != 0) continue;
            if (S_ISDIR(sst.st_mode)) { check_flags(sp); continue; }
            if (strcmp(de2->d_name + strlen(de2->d_name) - 2, ".c")) continue;
            FILE *f4 = fopen(sp, "r"); if (!f4) report_fatal_error_and_exit("cannot open file for flag scan");
            char *content = malloc(65536);
            if (!content) { fclose(f4); report_fatal_error_and_exit("malloc failed for flag scan"); }
            size_t cs = fread(content, 1, 65535, f4); fclose(f4);
            if (cs < 10 || cs >= 65535) { free(content); continue; }
            content[cs] = 0;
            const char *p = content;
            while ((p = strstr(p, "\"--")) != NULL) {
                p += 2;
                char flag[64]; int fi = 0;
                while (*p && *p != '"' && fi < 63) flag[fi++] = *p++;
                flag[fi] = 0;
                if (fi == 0) continue;
                if (!strstr(content, flag)) {
                    free(content); report_violation_with_actionable_hint(ERR_FLAG_NOT_IN_HELP, flag, 0, 0, sp);
                }
            }
            free(content);
        }
        closedir(dd);
    }
    check_flags(srcdir);
}
