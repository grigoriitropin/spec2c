// SPDX-License-Identifier: Apache-2.0
// load-operator-signed-exemption-table.c — load and verify exemption rules

#include "verify-structural-source-code-rules.h"
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
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
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
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

static void load_exemption_entries_into_table(cJSON *root) {
    cJSON *entries = cJSON_GetObjectItem(root, "entries");
    if (!entries) return;
    int sz = cJSON_GetArraySize(entries);
    for (int i = 0; i < sz && exemptions_count < 128; i++) {
        cJSON *item = cJSON_GetArrayItem(entries, i);
        cJSON *name_obj = cJSON_GetObjectItem(item, "exact_name");
        cJSON *scan_obj = cJSON_GetObjectItem(item, "scan_as");
        if (name_obj && scan_obj && name_obj->valuestring && scan_obj->valuestring) {
            snprintf(exemptions[exemptions_count].name, 128, "%s", name_obj->valuestring);
            snprintf(exemptions[exemptions_count].scan_as, 64, "%s", scan_obj->valuestring);
            exemptions_count++;
        }
    }
}

void load_operator_signed_exemption_table(const char *srcdir) {
    char *content = read_file_content_into_memory(srcdir);
    if (!content) {
        report_fatal_error_and_exit("cannot read operator-signed-exemption-name-table.json");
    }
    cJSON *root = cJSON_Parse(content);
    free(content);
    if (!root) {
        report_fatal_error_and_exit("cannot parse operator-signed-exemption-name-table.json");
    }
    load_exemption_entries_into_table(root);
    cJSON_Delete(root);
}

const char *match_name_against_exemption_table(const char *name) {
    for (int i = 0; i < exemptions_count; i++) {
        if (!strcmp(exemptions[i].name, name)) {
            return exemptions[i].scan_as;
        }
    }
    return NULL;
}
