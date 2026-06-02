// SPDX-License-Identifier: Apache-2.0
// legacy.c — old .spec.json format parser (backward compat)

#include "../common_h/share-type-definitions-across-files.h"

void parse_legacy_object_format_json(cJSON *root, spec_t *s) {
    memset(s, 0, sizeof(*s));
    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(j) || !j->valuestring[0]) report_fatal_error_and_exit("spec missing \"name\"");
    s->name = strdup(j->valuestring);
    if (!s->name) report_fatal_error_and_exit("strdup failed");
    s->ident = convert_hyphen_name_into_underscore(s->name);
    if (!s->ident) report_fatal_error_and_exit("strdup failed");
    j = cJSON_GetObjectItemCaseSensitive(root, "description");
    s->description = (cJSON_IsString(j) && j->valuestring[0])
        ? strdup(j->valuestring) : strdup("No description");
    if (!s->description) report_fatal_error_and_exit("strdup failed");
    cJSON *core = cJSON_GetObjectItemCaseSensitive(root, "core");
    if (core && cJSON_IsObject(core)) {
        j = cJSON_GetObjectItemCaseSensitive(core, "function");
        if (cJSON_IsString(j) && j->valuestring[0]) {
            s->core_function = strdup(j->valuestring);
            if (!s->core_function) report_fatal_error_and_exit("strdup failed");
        }
    }
    if (!s->core_function) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s_run", s->ident ? s->ident : s->name);
        s->core_function = strdup(buf);
        if (!s->core_function) report_fatal_error_and_exit("strdup failed");
    }
    cJSON *cmds = cJSON_GetObjectItemCaseSensitive(root, "commands");
    if (cmds && cJSON_IsObject(cmds) && cJSON_GetArraySize(cmds) > 0)
        s->has_commands = 1;
    cJSON *ckeys = cJSON_GetObjectItemCaseSensitive(root, "config_keys");
    if (ckeys && cJSON_IsArray(ckeys)) {
        s->nconfig_keys = cJSON_GetArraySize(ckeys);
        if (s->nconfig_keys > 0) {
            size_t total = 0;
            for (int i = 0; i < s->nconfig_keys; i++) {
                cJSON *k = cJSON_GetArrayItem(ckeys, i);
                if (cJSON_IsString(k)) total += strlen(k->valuestring) + 1;
            }
            s->config_keys_str = malloc(total + 1);
            if (!s->config_keys_str) report_fatal_error_and_exit("malloc");
            s->config_keys_str[0] = '\0';
            for (int i = 0; i < s->nconfig_keys; i++) {
                cJSON *k = cJSON_GetArrayItem(ckeys, i);
                if (i) strcat(s->config_keys_str, " ");
                strcat(s->config_keys_str, k->valuestring);
            }
        }
    }
    cJSON *dbq = cJSON_GetObjectItemCaseSensitive(root, "db_queries");
    if (dbq && cJSON_IsArray(dbq) && cJSON_GetArraySize(dbq) > 0)
        s->has_db = 1;
    cJSON_Delete(root);
}

void create_substitution_table_for_spec(const spec_t *s, subst_t *subs, int *nsubs) {
    *nsubs = 0;
#define ADD(k, fmt, ...) do { \
    subs[*nsubs].key = k; \
    snprintf(subs[*nsubs].val, VAL_SZ, fmt, ##__VA_ARGS__); \
    (*nsubs)++; \
} while(0)
    ADD("name", "%s", s->name);
    ADD("description", "%s", s->description);
    ADD("core_function", "%s", s->core_function);
    if (s->nconfig_keys > 0) {
        ADD("config_opt", " [--config <path>]");
        ADD("config_init", "    char *config_path = NULL;\n    int arg_off = 1;\n    if (argc > 2 && strcmp(argv[1], \"--config\") == 0) {\n        config_path = strdup(argv[2]);\n        if (!config_path) vl_die(\"%s\", \"strdup failed\");\n        arg_off = 3;\n    } else {\n        config_path = vl_default_config_path();\n        if (!config_path) vl_die(\"%s\", \"cannot resolve config path\");\n    }\n", s->name, s->name);
        ADD("config_cleanup", "    free(config_path);\n");
        ADD("config_usage_block", "        \"\\n\"\n        \"Config: ~/.config/vehir/env (override with --config <path>)\\n\"\n        \"  Required keys: %s\\n\"\n        \"  File must be chmod 600 (owner-only). Tokens never touch process env.\\n\"\n", s->config_keys_str ? s->config_keys_str : "");
    } else if (s->has_commands) {
        ADD("config_opt", "%s", "");
        ADD("config_init", "%s", "    int arg_off = 1;\n");
        ADD("config_cleanup", "%s", "");
        ADD("config_usage_block", "%s", "");
    } else {
        ADD("config_opt", "%s", "");
        ADD("config_init", "%s", "");
        ADD("config_cleanup", "%s", "");
        ADD("config_usage_block", "%s", "");
    }
    if (s->nconfig_keys > 0 && s->has_commands) {
        ADD("core_params_decl", ", int arg_off, const char *config_path");
        ADD("core_params_call", ", arg_off, config_path");
    } else if (s->nconfig_keys > 0) {
        ADD("core_params_decl", ", const char *config_path");
        ADD("core_params_call", ", config_path");
    } else if (s->has_commands) {
        ADD("core_params_decl", ", int arg_off");
        ADD("core_params_call", ", arg_off");
    } else {
        ADD("core_params_decl", "%s", "");
        ADD("core_params_call", "%s", "");
    }
    if (s->has_db) {
        ADD("db_includes", "#include \"db.h\"\n");
        ADD("db_init", "    vehir_db *db = vehir_db_open();\n    vl_db_check(\"%s\", db);\n", s->name);
        ADD("db_cleanup", "    vehir_db_close(db);\n");
    } else {
        ADD("db_includes", "%s", "");
        ADD("db_init", "%s", "    vehir_db *db = NULL;\n");
        ADD("db_cleanup", "%s", "");
    }
#undef ADD
}

char *apply_substitution_against_text_data(const char *tmpl, const subst_t *subs, int nsubs) {
    size_t len = strlen(tmpl);
    char *out = malloc(len * 2 + 4096);
    if (!out) report_fatal_error_and_exit("malloc");
    size_t pos = 0;
    const char *p = tmpl;
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            const char *end = strstr(p + 2, "}}");
            if (!end) { out[pos++] = *p++; continue; }
            size_t klen = (size_t)(end - (p + 2));
            int found = 0;
            for (int i = 0; i < nsubs; i++) {
                if (strlen(subs[i].key) == klen && strncmp(p + 2, subs[i].key, klen) == 0) {
                    size_t vlen = strlen(subs[i].val);
                    memcpy(out + pos, subs[i].val, vlen);
                    pos += vlen;
                    found = 1;
                    break;
                }
            }
            if (!found) { memcpy(out + pos, p, (size_t)(end + 2 - p)); pos += (size_t)(end + 2 - p); }
            p = end + 2;
        } else {
            out[pos++] = *p++;
        }
    }
    out[pos] = '\0';
    return out;
}
