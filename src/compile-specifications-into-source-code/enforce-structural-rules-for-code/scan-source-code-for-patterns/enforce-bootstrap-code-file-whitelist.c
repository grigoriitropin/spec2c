// SPDX-License-Identifier: Apache-2.0
// bootstrap C whitelist + external integrity manifest verification
#include "verify-structural-source-code-rules.h"
#include "verify-ed25519-digital-signature-key.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <cjson/cJSON.h>


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

/* Verify SHA256 hashes from external operator-signed integrity manifest */
static int load_operator_integrity_manifest_file(const char *srcdir, char **out, long *out_len) {
    char path[4096];
    FILE *f = fopen("operator-signed-integrity-manifest-hashes.json", "r");
    if (!f) {
        snprintf(path, sizeof(path), "%s/operator-signed-integrity-manifest-hashes.json", srcdir);
        f = fopen(path, "r");
    }
    if (!f) {
        snprintf(path, sizeof(path), "%s/../operator-signed-integrity-manifest-hashes.json", srcdir);
        f = fopen(path, "r");
    }
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    *out_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (*out_len < 0) { fclose(f); return 0; }
    *out = malloc(*out_len + 1);
    if (!*out) { fclose(f); return 0; }
    size_t rd = fread(*out, 1, *out_len, f);
    fclose(f);
    (*out)[rd] = 0;
    *out_len = (long)rd;
    return 1;
}

static int extract_signed_integrity_payload_bytes(const char *raw, char *out, int outsz) {
    cJSON *root = cJSON_Parse(raw);
    if (!root) return 0;
    cJSON_DeleteItemFromObject(root, "signature_hex");
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return 0;
    int plen = (int)strlen(payload);
    if (plen < outsz) memcpy(out, payload, plen + 1);
    else plen = 0;
    free(payload);
    return plen;
}

static void verify_manifest_entry_file_hash(const char *srcdir, const char *fname, const char *expected) {
    char found[8192] = {0};
    if (!search_file_using_name_recursive(srcdir, fname, found, sizeof(found)))
        return;
    FILE *f2 = fopen(found, "rb");
    if (!f2) return;
    fseek(f2, 0, SEEK_END);
    long fsz = ftell(f2);
    fseek(f2, 0, SEEK_SET);
    if (fsz <= 0 || fsz > 1048576) { fclose(f2); return; }
    uint8_t *buf = malloc((size_t)fsz);
    if (!buf) { fclose(f2); return; }
    size_t n = fread(buf, 1, (size_t)fsz, f2);
    fclose(f2);
    uint8_t hash[32];
    compute_sha256_hash_into_bytes(buf, (uint32_t)n, hash);
    char actual_hex[65];
    for (int j = 0; j < 32; j++)
        snprintf(actual_hex + j*2, 3, "%02x", hash[j]);
    actual_hex[64] = 0;
    free(buf);
    if (strcmp(actual_hex, expected) != 0) {
        fprintf(stderr, "spec2c: SOUL §7: file %s SHA256 mismatch — modified\n"
            "  → expected %s\n  → actual   %s\n",
            fname, expected, actual_hex);
        exit(1);
    }
}

void enforce_bootstrap_code_freeze_check(const char *srcdir) {
    char *content = NULL;
    long content_len = 0;
    if (!load_operator_integrity_manifest_file(srcdir, &content, &content_len))
        return;
    cJSON *root = cJSON_Parse(content);
    if (!root) { free(content); return; }
    cJSON *pub = cJSON_GetObjectItem(root, "public_key_hex");
    cJSON *sig = cJSON_GetObjectItem(root, "signature_hex");
    if (!pub || !sig || !pub->valuestring || !sig->valuestring) {
        cJSON_Delete(root);
        free(content);
        return;
    }
    char pk[128]; snprintf(pk, sizeof(pk), "%s", pub->valuestring);
    char sh[256]; snprintf(sh, sizeof(sh), "%s", sig->valuestring);
    char payload[16384];
    int plen = extract_signed_integrity_payload_bytes(content, payload, sizeof(payload));
    if (!plen || verify_signature(pk, sh, (unsigned char *)payload, plen) != 0) {
        fprintf(stderr, "spec2c: integrity manifest: Ed25519 signature invalid\n");
        cJSON_Delete(root); free(content); exit(1);
    }
    cJSON *entries = cJSON_GetObjectItem(root, "entries");
    if (entries) {
        int sz = cJSON_GetArraySize(entries);
        for (int i = 0; i < sz; i++) {
            cJSON *item = cJSON_GetArrayItem(entries, i);
            cJSON *fn = cJSON_GetObjectItem(item, "file");
            cJSON *hs = cJSON_GetObjectItem(item, "sha256");
            if (fn && hs && fn->valuestring && hs->valuestring)
                verify_manifest_entry_file_hash(srcdir, fn->valuestring, hs->valuestring);
        }
    }
    cJSON_Delete(root);
    free(content);
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
