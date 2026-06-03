// SPDX-License-Identifier: Apache-2.0
// Recursive directory scanner with structural enforcement checks
#include "verify-structural-source-code-rules.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void scan_each_directory_with_checks(const char *dirpath,
    fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty) {
    DIR *d = opendir(dirpath); if (!d) return;
    int file_cnt = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dirpath, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            validate_name_against_soul_rules("directory", de->d_name, sub);
            scan_each_directory_with_checks(sub, fns, fn_qty, incs, inc_qty);
            continue;
        }
        if (!match_source_code_header_filename(de->d_name)) {
            size_t dn_len = strlen(de->d_name);
            int is_ipm = dn_len > 4 && !strcmp(de->d_name + dn_len - 4, ".ipm");
            if (!is_ipm) {
                if (!check_non_source_file_allowlist(de->d_name)) {
                    fprintf(stderr, "FATAL: non-source file in %s: %s\n", dirpath, de->d_name);
                    exit(1);
                }
                continue;
            }
        }
        if (!match_name_against_bootstrap_list(de->d_name)) {
            size_t dn_len2 = strlen(de->d_name);
            int is_ipm2 = dn_len2 > 4 && !strcmp(de->d_name + dn_len2 - 4, ".ipm");
            if (!is_ipm2)
                report_violation_with_actionable_hint(ERR_NOT_IN_BOOTSTRAP_WHITELIST, sub, 0, 0, NULL);
        }
        file_cnt++;
        char fname[256]; snprintf(fname, sizeof(fname), "%s", de->d_name);
        char *dot = strrchr(fname, '.'); if (dot) *dot = 0;
        validate_name_against_soul_rules("file", fname, sub);
        int is_c = !strcmp(de->d_name + strlen(de->d_name) - 2, ".c");
        int is_source = is_c || (strlen(de->d_name) > 4 && !strcmp(de->d_name + strlen(de->d_name) - 4, ".ipm"));
        check_single_file_for_violations(sub, is_c, is_source, fns, fn_qty, incs, inc_qty);
    }
    closedir(d);
    if (file_cnt > MAX_FILES_PER_DIR)
        report_violation_with_actionable_hint(ERR_TOO_MANY_FILES_IN_DIR, dirpath, file_cnt, MAX_FILES_PER_DIR, NULL);
}
