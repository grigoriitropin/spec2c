// SPDX-License-Identifier: Apache-2.0
// ipm-meta — extract metadata from .ipm file, output meta.json
// Usage: ipm-meta <file.ipm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "ipm_time.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f); buf[sz] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    ipm_time_init();
    if (argc < 2) {
        fprintf(stderr, "Usage: ipm-meta <file.ipm>\n");
        return 1;
    }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    cJSON *spec = cJSON_Parse(text);
    free(text);
    if (!spec) { fprintf(stderr, "JSON parse error\n"); return 1; }

    cJSON *meta = cJSON_CreateObject();

    /* source name */
    cJSON *pkg = cJSON_GetObjectItemCaseSensitive(spec, "package_name");
    if (cJSON_IsString(pkg))
        cJSON_AddItemToObject(meta, "source", cJSON_Duplicate(pkg, 1));

    /* module */
    cJSON *mod = cJSON_GetObjectItemCaseSensitive(spec, "module_name");
    if (cJSON_IsString(mod))
        cJSON_AddItemToObject(meta, "module", cJSON_Duplicate(mod, 1));

    /* functions */
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        cJSON_AddNumberToObject(meta, "function_count", cJSON_GetArraySize(funcs));

        cJSON *names = cJSON_CreateArray();
        cJSON *fn = funcs->child;
        int total_instr = 0;
        while (fn) {
            cJSON_AddItemToArray(names, cJSON_CreateString(fn->string));
            cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
            if (body && cJSON_IsArray(body))
                total_instr += cJSON_GetArraySize(body);
            fn = fn->next;
        }
        cJSON_AddItemToObject(meta, "function_names", names);
        cJSON_AddNumberToObject(meta, "total_instructions", total_instr);
    }

    /* imports */
    cJSON *imps = cJSON_GetObjectItemCaseSensitive(spec, "imports");
    if (imps && cJSON_IsArray(imps)) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < cJSON_GetArraySize(imps); i++)
            cJSON_AddItemToArray(arr, cJSON_Duplicate(cJSON_GetArrayItem(imps, i), 1));
        cJSON_AddItemToObject(meta, "imports", arr);
    }

    /* toolchain provenance */
    cJSON_AddStringToObject(meta, "toolchain", "spec2c");
    cJSON_AddStringToObject(meta, "toolchain_version", "0.2.0");

    char ts[21]; ipm_timestamp(ts); cJSON_AddStringToObject(meta, "timestamp", ts);
    cJSON_AddNumberToObject(meta, "elapsed_us", (double)ipm_time_us());
    char *out = cJSON_PrintUnformatted(meta);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(meta);
    cJSON_Delete(spec);
    return 0;
}
