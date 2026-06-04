// SPDX-License-Identifier: Apache-2.0
#include "../verify-structural-source-code-rules.h"
#include "../../../../verify-ed25519-digital-signature-key.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#define PUBKEY_HEX "489532082ae4dfc21c6ffe21e1bf78c432bc07200d712ad07568c9a46fe52f24"

typedef struct {
    char name[128];
    char scan_as[64];
} exemption_entry_t;

static exemption_entry_t exemptions[128];
static int exemptions_count = 0;

static char *read_file_content_into_memory(const char *srcdir) {
    char path[4096];
    FILE *f = fopen("operator-signed-exemption-name-table.json", "r");
    if (!f) {
        snprintf(path, sizeof(path), "%s/../operator-signed-exemption-name-table.json", srcdir);
        f = fopen(path, "r");
    }
    if (!f) {
        snprintf(path, sizeof(path), "%s/operator-signed-exemption-name-table.json", srcdir);
        f = fopen(path, "r");
    }
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f); fclose(f);
    buf[rd] = '\0';
    return buf;
}

void load_operator_signed_exemption_table(const char *srcdir) {
    char *content = read_file_content_into_memory(srcdir);
    if (!content)
        report_fatal_error_and_exit("cannot read operator-signed-exemption-name-table.json");
    char sig_hex[256] = {0};
    char path[4096];
    snprintf(path, sizeof(path), "%s/../operator-signed-exemption-name-table.sig", srcdir);
    FILE *sf = fopen(path, "r");
    if (!sf) { snprintf(path, sizeof(path), "%s/operator-signed-exemption-name-table.sig", srcdir); sf = fopen(path, "r"); }
    if (!sf) { snprintf(path, sizeof(path), "operator-signed-exemption-name-table.sig"); sf = fopen(path, "r"); }
    if (!sf) { free(content);
        report_fatal_error_and_exit("cannot read exemption table signature file"); }
    size_t sn = fread(sig_hex, 1, 255, sf); fclose(sf); sig_hex[sn] = 0;
    if (sn < 128) { free(content);
        report_fatal_error_and_exit("exemption table signature too short"); }
    long clen = (long)strlen(content);
    if (verify_signature(PUBKEY_HEX, sig_hex, (unsigned char *)content, (size_t)clen) != 0)
        { free(content); report_fatal_error_and_exit("exemption table: Ed25519 signature invalid"); }
    cJSON *root = cJSON_Parse(content);
    free(content);
    if (!root) report_fatal_error_and_exit("exemption table: JSON parse failed");
    cJSON *entries = cJSON_GetObjectItem(root, "entries");
    if (entries) {
        int sz = cJSON_GetArraySize(entries);
        for (int i = 0; i < sz && exemptions_count < 128; i++) {
            cJSON *item = cJSON_GetArrayItem(entries, i);
            cJSON *n = cJSON_GetObjectItem(item, "exact_name");
            cJSON *s = cJSON_GetObjectItem(item, "scan_as");
            if (n && s && n->valuestring && s->valuestring) {
                snprintf(exemptions[exemptions_count].name, 128, "%s", n->valuestring);
                snprintf(exemptions[exemptions_count].scan_as, 64, "%s", s->valuestring);
                exemptions_count++;
            }
        }
    }
    cJSON_Delete(root);
}

const char *match_name_against_exemption_table(const char *name) {
    for (int i = 0; i < exemptions_count; i++)
        if (!strcmp(exemptions[i].name, name)) return exemptions[i].scan_as;
    return NULL;
}
