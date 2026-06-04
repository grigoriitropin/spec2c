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
static char hex_buf[65];

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
        if (!strcmp(de->d_name, "bootstrap-file-sha-hashes-generated.h")) continue;
        if (!strcmp(de->d_name, "bootstrap-freeze-data-compiled-into.h")) continue;
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

int main(void) {
    cJSON *arr = cJSON_CreateArray();
    walk_collect("src", "src", arr);
    /* Add flake.nix */
    char hash[65];
    file_sha256_hex("flake.nix", hash);
    if (hash[0]) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "file", "flake.nix");
        cJSON_AddStringToObject(entry, "sha256", hash);
        cJSON_AddStringToObject(entry, "note", "build-entry-pin");
        cJSON_AddItemToArray(arr, entry);
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "entries", arr);
    char *json = cJSON_PrintUnformatted(root);
    if (json) { printf("%s\n", json); free(json); }
    cJSON_Delete(root);
    return 0;
}
