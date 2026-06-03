// SPDX-License-Identifier: Apache-2.0
// bootstrap C whitelist + SHA256 hash verification — HASHES COMPILED INTO BINARY
#include "verify-structural-source-code-rules.h"
#include "../bootstrap-compiled-limit-hash-data/bootstrap-file-sha-hashes-generated.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>



extern void compute_sha256_hash_into_bytes(const uint8_t *data, uint32_t len, uint8_t out[32]);

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
    for (int i = 0; i < whitelist_count_now; i++) {
        /* match against basename of whitelist entry (strip path prefix) */
        const char *bn = strrchr(whitelist_names[i], '/');
        bn = bn ? bn + 1 : whitelist_names[i];
        if (!strcmp(bn, basename)) return 1;
    }
    return 0;
}

/* find a file by basename anywhere under dpath */
static int search_file_using_name_recursive(const char *dpath, const char *target, char *out, size_t outsz) {
    DIR *d = opendir(dpath);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192]; snprintf(sub, sizeof(sub), "%s/%s", dpath, de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (search_file_using_name_recursive(sub, target, out, outsz)) { closedir(d); return 1; }
            continue;
        }
        if (!strcmp(de->d_name, target)) {
            snprintf(out, outsz, "%s", sub);
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

/* Verify SHA256 hashes of bootstrap files — hashes compiled into binary */
void enforce_bootstrap_code_freeze_check(const char *srcdir) {
    for (int i = 0; i < BOOTSTRAP_HASH_COUNT; i++) {
        char found[8192] = {0};
        if (!search_file_using_name_recursive(srcdir, hash_file_names[i], found, sizeof(found)))
            continue;
        /* Compute actual SHA256 of the file */
        FILE *f2 = fopen(found, "rb");
        if (!f2) continue;
        fseek(f2, 0, SEEK_END);
        long sz = ftell(f2);
        fseek(f2, 0, SEEK_SET);
        if (sz <= 0 || sz > 1048576) { fclose(f2); continue; }
        uint8_t *buf = malloc((size_t)sz);
        if (!buf) { fclose(f2); continue; }
        size_t n = fread(buf, 1, (size_t)sz, f2);
        fclose(f2);
        uint8_t hash[32];
        compute_sha256_hash_into_bytes(buf, (uint32_t)n, hash);
        char actual_hex[65];
        for (int j = 0; j < 32; j++)
            snprintf(actual_hex + j*2, 3, "%02x", hash[j]);
        actual_hex[64] = 0;
        free(buf);
        if (strcmp(actual_hex, hash_sha256_values[i]) != 0) {
            fprintf(stderr, "spec2c: SOUL §7: bootstrap file %s SHA256 mismatch — file was modified\n"
                "  → expected %s\n"
                "  → actual   %s\n"
                "  → hashes are compiled into the binary, rewrite changes as IPM module\n",
                hash_file_names[i], hash_sha256_values[i], actual_hex);
            exit(1);
        }
    }


}
// rebuild

static char non_source_files[64][256];
static int non_source_files_qty;

void load_non_source_file_allowlist(const char *srcdir) {
    char path[4096]; snprintf(path, sizeof(path), "%s/allowed-non-source-files.txt", srcdir);
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "FATAL: cannot open allowed-non-source-files.txt\n"); exit(1); }
    char line[256];
    while (fgets(line, sizeof(line), f) && non_source_files_qty < 64) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0 && line[0] != '#')
            snprintf(non_source_files[non_source_files_qty++], 256, "%s", line);
    }
    fclose(f);
}
int check_non_source_file_allowlist(const char *name) {
    for (int i = 0; i < non_source_files_qty; i++)
        if (!strcmp(non_source_files[i], name)) return 1;
    return 0;
}
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
