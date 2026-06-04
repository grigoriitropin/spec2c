// SPDX-License-Identifier: Apache-2.0
// generate-integrity-manifest-from-source.c — emit JSON manifest from source tree
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <cjson/cJSON.h>

void compute_sha256_hash_into_bytes(const uint8_t *data, uint32_t len, uint8_t out[32]);

static void file_sha256_hex(const char *path, char out[65]) {
    FILE *f = fopen(path, "rb");
    if (!f) { out[0] = 0; return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1048576) { fclose(f); out[0] = 0; return; }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); out[0] = 0; return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    uint8_t hash[32];
    compute_sha256_hash_into_bytes(buf, (uint32_t)n, hash);
    free(buf);
    for (int i = 0; i < 32; i++)
        sprintf(out + i*2, "%02x", hash[i]);
    out[64] = 0;
}

static void walk_collect(const char *dpath, const char *prefix, cJSON *arr) {
    DIR *d = opendir(dpath);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192];
        snprintf(sub, sizeof(sub), "%s/%s", dpath, de->d_name);
        char rel[8192];
        if (prefix[0])
            snprintf(rel, sizeof(rel), "%s/%s", prefix, de->d_name);
        else
            snprintf(rel, sizeof(rel), "%s", de->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { walk_collect(sub, rel, arr); continue; }
        size_t nl = strlen(de->d_name);
        int is_c = nl > 2 && !strcmp(de->d_name + nl - 2, ".c");
        int is_h = nl > 2 && !strcmp(de->d_name + nl - 2, ".h");
        if (!is_c && !is_h) continue;
        char hash[65];
        file_sha256_hex(sub, hash);
        if (!hash[0]) continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "file", rel);
        cJSON_AddStringToObject(entry, "sha256", hash);
        cJSON_AddItemToArray(arr, entry);
    }
    closedir(d);
}

static int manifest_has_file(cJSON *arr, const char *relpath) {
    int sz = cJSON_GetArraySize(arr);
    for (int i = 0; i < sz; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *fn = cJSON_GetObjectItem(item, "file");
        if (fn && fn->valuestring && !strcmp(fn->valuestring, relpath)) return 1;
    }
    return 0;
}

static void add_file_to_manifest(cJSON *arr, const char *relpath, const char *note) {
    char hash[65];
    file_sha256_hex(relpath, hash);
    if (!hash[0]) return;
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "file", relpath);
    cJSON_AddStringToObject(entry, "sha256", hash);
    if (note) cJSON_AddStringToObject(entry, "note", note);
    cJSON_AddItemToArray(arr, entry);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int find_file_by_path_suffix(const char *dpath, const char *prefix, const char *suffix, char *out, size_t outsz) {
    DIR *d = opendir(dpath);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char sub[8192];
        snprintf(sub, sizeof(sub), "%s/%s", dpath, de->d_name);
        char rel[8192];
        if (prefix[0])
            snprintf(rel, sizeof(rel), "%s/%s", prefix, de->d_name);
        else
            snprintf(rel, sizeof(rel), "%s", de->d_name);
        size_t rl = strlen(rel), sl = strlen(suffix);
        if (rl >= sl && !strcmp(rel + rl - sl, suffix)) {
            snprintf(out, outsz, "%s", rel);
            closedir(d);
            return 1;
        }
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (find_file_by_path_suffix(sub, rel, suffix, out, outsz)) {
                closedir(d);
                return 1;
            }
        }
    }
    closedir(d);
    return 0;
}

static void resolve_whitelist_path(const char *line, const char *whitelist_dir, char *out, size_t outsz) {
    char full[8192];
    snprintf(full, sizeof(full), "source-code-for-compiler-generation/%s", line);
    if (file_exists(full)) { snprintf(out, outsz, "%s", full); return; }
    if (find_file_by_path_suffix(whitelist_dir, whitelist_dir, line, out, outsz)) return;
    const char *bn = strrchr(line, '/');
    const char *name = bn ? bn + 1 : line;
    snprintf(full, sizeof(full), "%s", name);
    if (file_exists(full)) { snprintf(out, outsz, "%s", full); return; }
    snprintf(full, sizeof(full), "source-code-for-compiler-generation/%s", name);
    if (file_exists(full)) { snprintf(out, outsz, "%s", full); return; }
}

static int auto_include_whitelist_files(cJSON *arr, const char *whitelist_path, const char *whitelist_dir) {
    FILE *f = fopen(whitelist_path, "r");
    if (!f) {
        fprintf(stderr, "FATAL: cannot open whitelist: %s\n", whitelist_path);
        exit(1);
    }
    int added = 0, present = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (l == 0 || line[0] == '#') continue;
        char resolved[8192];
        resolve_whitelist_path(line, whitelist_dir, resolved, sizeof(resolved));
        if (!file_exists(resolved)) continue;
        if (manifest_has_file(arr, resolved)) {
            present++;
            continue;
        }
        add_file_to_manifest(arr, resolved, "bootstrap-whitelist-entry");
        added++;
    }
    fclose(f);
    fprintf(stderr, "whitelist auto-include: %d added, %d already present\n", added, present);
    int total = added + present;
    f = fopen(whitelist_path, "r");
    if (!f) { fprintf(stderr, "FATAL: whitelist re-open failed\n"); exit(1); }
    int fatal = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = 0;
        if (l == 0 || line[0] == '#') continue;
        char resolved[8192];
        resolve_whitelist_path(line, whitelist_dir, resolved, sizeof(resolved));
        if (!file_exists(resolved)) continue;
        if (!manifest_has_file(arr, resolved)) {
            fprintf(stderr, "FATAL: whitelist file exists on disk but NOT in manifest: %s\n", line);
            fatal++;
        }
    }
    fclose(f);
    if (fatal) {
        fprintf(stderr, "FATAL: %d whitelist file(s) missing from integrity manifest\n", fatal);
        exit(1);
    }
    return total;
}

int main(void) {
    cJSON *arr = cJSON_CreateArray();
    walk_collect("source-code-for-compiler-generation", "source-code-for-compiler-generation", arr);
    /* Trust-critical root files — must be frozen */
    add_file_to_manifest(arr, "verify-ed25519-digital-signature-key.c", "signature-verifier");
    add_file_to_manifest(arr, "verify-ed25519-digital-signature-key.h", "signature-verifier-header");
    add_file_to_manifest(arr, "enforce-link-time-whitelisted-symbols.c", "L2-symbol-gate");
    add_file_to_manifest(arr, "generate-integrity-manifest-from-source.c", "manifest-generator");
    add_file_to_manifest(arr, "check-banned-patterns-pure-ipm.ipm", "ipm-banned-patterns");
    add_file_to_manifest(arr, "enforce-naming-rules-via-ffi.ipm", "ipm-naming-rules");
    /* Policy .txt files — whitelist growth gated */
    add_file_to_manifest(arr, "source-code-for-compiler-generation/allowed-names.txt", "allowed-names");
    add_file_to_manifest(arr, "source-code-for-compiler-generation/banned-patterns.txt", "banned-patterns");
    add_file_to_manifest(arr, "source-code-for-compiler-generation/bootstrap-c-whitelist.txt", "bootstrap-whitelist");
    add_file_to_manifest(arr, "source-code-for-compiler-generation/bootstrap-c-freeze-limits.txt", "freeze-limits");
    add_file_to_manifest(arr, "source-code-for-compiler-generation/allowed-non-source-files.txt", "non-source");
    /* Build entry */
    add_file_to_manifest(arr, "flake.nix", "build-entry-pin");
    /* Auto-include every bootstrap-whitelist file that exists on disk */
    auto_include_whitelist_files(arr,
        "source-code-for-compiler-generation/bootstrap-c-whitelist.txt",
        "source-code-for-compiler-generation");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "entries", arr);
    char *json = cJSON_PrintUnformatted(root);
    if (json) { printf("%s\n", json); free(json); }
    cJSON_Delete(root);
    return 0;
}
