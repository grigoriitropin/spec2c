// SPDX-License-Identifier: Apache-2.0
#include "verify-structural-source-code-rules.h"
#include "verify-ed25519-digital-signature-key.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct {
    char name[128];
    char scan_as[64];
} exemption_entry_t;

static exemption_entry_t exemptions[128];
static int exemptions_count = 0;

static void report_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "spec2c: %s\n", msg); exit(1);
}

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

static int extract_signed_payload_bytes_raw(const char *raw, char *out, size_t outsz) {
    int len = 0;
    const char *p = raw;
    while (*p) {
        if (!strncmp(p, ",\"signature_hex\":", 17) || !strncmp(p, ",\"signed_bytes_sha256\":", 23)) {
            while (*p && *p != '"') p++;
            if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p == '"') p++; }
            while (*p && *p != '"') p++;
            if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p == '"') p++; }
        } else {
            if (len < (int)outsz - 1) out[len++] = *p;
            p++;
        }
    }
    out[len] = 0;
    return len;
}

void load_operator_signed_exemption_table(const char *srcdir) {
    char *content = read_file_content_into_memory(srcdir);
    if (!content)
        report_fatal_error_and_exit("cannot read operator-signed-exemption-name-table.json");
    cJSON *root = cJSON_Parse(content);
    if (!root) { free(content);
        report_fatal_error_and_exit("cannot parse operator-signed-exemption-name-table.json"); }
    cJSON *pub_obj = cJSON_GetObjectItem(root, "public_key_hex");
    cJSON *sig_obj = cJSON_GetObjectItem(root, "signature_hex");
    if (!pub_obj || !sig_obj || !pub_obj->valuestring || !sig_obj->valuestring)
        { cJSON_Delete(root); free(content);
          report_fatal_error_and_exit("operator-signed-exemption-name-table.json missing pubkey/sig"); }
    char pk_hex[128]; snprintf(pk_hex, sizeof(pk_hex), "%s", pub_obj->valuestring);
    char sig_hex[256]; snprintf(sig_hex, sizeof(sig_hex), "%s", sig_obj->valuestring);
    char signed_bytes[4096];
    int sb_len = extract_signed_payload_bytes_raw(content, signed_bytes, sizeof(signed_bytes));
    if (verify_signature(pk_hex, sig_hex, (unsigned char *)signed_bytes, sb_len) != 0) {
        cJSON_Delete(root); free(content);
        report_fatal_error_and_exit("exemption table: Ed25519 signature invalid");
    }
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
    cJSON_Delete(root); free(content);
}

const char *match_name_against_exemption_table(const char *name) {
    for (int i = 0; i < exemptions_count; i++)
        if (!strcmp(exemptions[i].name, name)) return exemptions[i].scan_as;
    return NULL;
}
