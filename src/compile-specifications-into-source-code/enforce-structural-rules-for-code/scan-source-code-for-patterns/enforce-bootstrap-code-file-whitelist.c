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
