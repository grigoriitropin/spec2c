// SPDX-License-Identifier: Apache-2.0
// ipm-graph — dependency graph builder from filesystem scan + meta.json
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct {
    char *name;
    char *value;
} kv_t;

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

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : ".";

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "scan-filesystem %s 2>/dev/null", root);
    FILE *fp = popen(cmd, "r");
    if (!fp) { fprintf(stderr, "ipm-graph: cannot run scan-filesystem\n"); return 1; }

    cJSON *graph = cJSON_CreateObject();
    cJSON_AddStringToObject(graph, "root", root);
    cJSON *nodes = cJSON_CreateArray();
    cJSON *counts = cJSON_CreateObject();

    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        cJSON *entry = cJSON_Parse(line);
        if (!entry) continue;

        const char *path = cJSON_GetObjectItemCaseSensitive(entry, "path")->valuestring;
        const char *type = cJSON_GetObjectItemCaseSensitive(entry, "type")->valuestring;
        const char *cat = category(path, type);

        cJSON_AddItemToArray(nodes,
            cJSON_CreateString(cJSON_PrintUnformatted(entry)));

        /* count by category */
        cJSON *cnt = cJSON_GetObjectItemCaseSensitive(counts, cat);
        if (!cnt) { cJSON_AddNumberToObject(counts, cat, 1); }
        else { cJSON_SetNumberValue(cnt, cnt->valueint + 1); }

        cJSON_Delete(entry);
    }
    pclose(fp);

    cJSON_AddItemToObject(graph, "counts", counts);
    cJSON_AddNumberToObject(graph, "total_nodes", cJSON_GetArraySize(nodes));

    char *out = cJSON_Print(graph);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(graph);
    return 0;
}
