// SPDX-License-Identifier: Apache-2.0
// ipm-graph — pure filter: reads JSON lines from stdin, outputs JSON graph
// Usage: scan-filesystem /path | ipm-graph
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "ipm_time.h"

static int known_configs(const char *name) {
    const char *known[] = {
        "AGENTS.md", "SOUL.md", "CLAUDE.md", "GEMINI.md",
        "flake.nix", "flake.lock", "Makefile", "CMakeLists.txt",
        "package.json", "Cargo.toml", "go.mod",
        ".gitignore", ".gitattributes",
        "LICENSE", "NOTICE", "README.md",
        NULL
    };
    for (const char **k = known; *k; k++)
        if (!strcmp(name, *k)) return 1;
    return 0;
}

static const char *category(const char *path, const char *type) {
    if (!strcmp(type, "dir")) {
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        if (!strcmp(base, ".git")) return "vcs";
        if (!strcmp(base, "node_modules")) return "dependency";
        if (!strcmp(base, ".cache")) return "cache";
        if (base[0] == '.') return "hidden";
        return "directory";
    }
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (known_configs(name)) return "config";
    const char *ext = strrchr(name, '.');
    if (ext) {
        if (!strcmp(ext, ".c") || !strcmp(ext, ".h")) return "c_source";
        if (!strcmp(ext, ".ipm")) return "ipm_source";
        if (!strcmp(ext, ".nix")) return "nix_config";
        if (!strcmp(ext, ".json")) return "json_data";
        if (!strcmp(ext, ".md")) return "documentation";
        if (!strcmp(ext, ".sh")) return "script";
    }
    return "other";
}

int main(void) {
    ipm_time_init();
    cJSON *counts = cJSON_CreateObject();

    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        cJSON *entry = cJSON_Parse(line);
        if (!entry) continue;

        cJSON *path_j = cJSON_GetObjectItemCaseSensitive(entry, "path");
        cJSON *type_j = cJSON_GetObjectItemCaseSensitive(entry, "type");
        if (!path_j || !type_j) { cJSON_Delete(entry); continue; }

        const char *cat = category(path_j->valuestring, type_j->valuestring);
        cJSON *cnt = cJSON_GetObjectItemCaseSensitive(counts, cat);
        if (!cnt) cJSON_AddNumberToObject(counts, cat, 1);
        else cJSON_SetNumberValue(cnt, cnt->valueint + 1);

        cJSON_Delete(entry);
    }

    cJSON *graph = cJSON_CreateObject();
    char ts[21]; ipm_timestamp(ts); cJSON_AddStringToObject(graph, "timestamp", ts);
    cJSON_AddNumberToObject(graph, "elapsed_us", (double)ipm_time_us());
    cJSON_AddItemToObject(graph, "counts", counts);
    cJSON_AddNumberToObject(graph, "total_nodes",
        cJSON_GetArraySize(counts) > 0 ? 1 : 0); /* approximate */

    char *out = cJSON_Print(graph);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(graph);
    return 0;
}
