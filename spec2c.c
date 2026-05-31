// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin
//
// spec2c v0.2 — declarative C skeleton generator
//
// Reads skeleton.json (section manifest) + tool.spec.json → substitutes
// {{key}} placeholders from computed values → emits C scaffold.
// Templates are pure C + placeholders; zero logic in templates.
// Shared logic lives in vehir_lib (vl_die, vl_cfg_*, vl_db_*, vl_safe_exec).

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cjson/cJSON.h>

#define SUBST_MAX 32
#define VAL_SZ   4096

typedef struct {
    const char *key;
    char        val[VAL_SZ];
} subst_t;

typedef struct {
    const char *name;
    const char *ident;
    const char *description;
    const char *core_function;
    int         nconfig_keys;
    int         has_commands;
    char       *config_keys_str;
    int         has_db;
} spec_t;

static _Noreturn void die(const char *msg) {
    cJSON *r = cJSON_CreateObject();
    if (!r) { fprintf(stderr, "spec2c: FATAL: cJSON alloc failed\n"); exit(1); }
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", msg);
    char *s = cJSON_PrintUnformatted(r);
    if (s) { printf("%s\n", s); free(s); }
    cJSON_Delete(r);
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
}

static char *name_to_ident(const char *name) {
    char *s = strdup(name);
    if (!s) return NULL;
    for (char *p = s; *p; p++)
        if (*p == '-') *p = '_';
    return s;
}

static char *read_file(const char *path) {
    FILE *f;
    if (strcmp(path, "-") == 0) f = stdin;
    else { f = fopen(path, "r"); if (!f) die("cannot open file"); }
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) die("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); die("realloc"); } buf = t; }
    }
    if (ferror(f)) { free(buf); die("read error"); }
    buf[len] = '\0';
    if (f != stdin) fclose(f);
    return buf;
}

static void parse_spec(const char *text, spec_t *s) {
    memset(s, 0, sizeof(*s));
    cJSON *root = cJSON_Parse(text);
    if (!root) die("JSON parse error in spec");

    cJSON *j = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(j) || !j->valuestring[0]) die("spec missing \"name\"");
    s->name = strdup(j->valuestring);
    if (!s->name) die("strdup failed");
    s->ident = name_to_ident(s->name);
    if (!s->ident) die("strdup failed");

    j = cJSON_GetObjectItemCaseSensitive(root, "description");
    s->description = (cJSON_IsString(j) && j->valuestring[0])
        ? strdup(j->valuestring) : strdup("No description");
    if (!s->description) die("strdup failed");

    cJSON *core = cJSON_GetObjectItemCaseSensitive(root, "core");
    if (core && cJSON_IsObject(core)) {
        j = cJSON_GetObjectItemCaseSensitive(core, "function");
        if (cJSON_IsString(j) && j->valuestring[0]) {
            s->core_function = strdup(j->valuestring);
            if (!s->core_function) die("strdup failed");
        }
    }
    if (!s->core_function) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s_run", s->ident ? s->ident : s->name);
        s->core_function = strdup(buf);
        if (!s->core_function) die("strdup failed");
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
            if (!s->config_keys_str) die("malloc");
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

static void compute_substs(const spec_t *s, subst_t *subs, int *nsubs) {
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
        ADD("config_init",
            "    char *config_path = NULL;\n"
            "    int arg_off = 1;\n"
            "    if (argc > 2 && strcmp(argv[1], \"--config\") == 0) {\n"
            "        config_path = strdup(argv[2]);\n"
            "        if (!config_path) vl_die(\"%s\", \"strdup failed\");\n"
            "        arg_off = 3;\n"
            "    } else {\n"
            "        config_path = vl_default_config_path();\n"
            "        if (!config_path) vl_die(\"%s\", \"cannot resolve config path\");\n"
            "    }\n", s->name, s->name);
        ADD("config_cleanup", "    free(config_path);\n");
        ADD("config_usage_block",
            "        \"\\n\"\n"
            "        \"Config: ~/.config/vehir/env (override with --config <path>)\\n\"\n"
            "        \"  Required keys: %s\\n\"\n"
            "        \"  File must be chmod 600 (owner-only). Tokens never touch process env.\\n\"\n",
            s->config_keys_str ? s->config_keys_str : "");
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
        ADD("db_init",
            "    vehir_db *db = vehir_db_open();\n"
            "    vl_db_check(\"%s\", db);\n", s->name);
        ADD("db_cleanup", "    vehir_db_close(db);\n");
    } else {
        ADD("db_includes", "%s", "");
        ADD("db_init", "%s", "    vehir_db *db = NULL;\n");
        ADD("db_cleanup", "%s", "");
    }
#undef ADD
}

static char *subst_apply(const char *tmpl, const subst_t *subs, int nsubs) {
    size_t len = strlen(tmpl);
    char *out = malloc(len * 2 + 4096);
    if (!out) die("malloc");
    size_t pos = 0;

    const char *p = tmpl;
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            const char *end = strstr(p + 2, "}}");
            if (!end) { out[pos++] = *p++; continue; }
            size_t klen = (size_t)(end - (p + 2));
            int found = 0;
            for (int i = 0; i < nsubs; i++) {
                if (strlen(subs[i].key) == klen &&
                    strncmp(p + 2, subs[i].key, klen) == 0) {
                    size_t vlen = strlen(subs[i].val);
                    memcpy(out + pos, subs[i].val, vlen);
                    pos += vlen;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                memcpy(out + pos, p, (size_t)(end + 2 - p));
                pos += (size_t)(end + 2 - p);
            }
            p = end + 2;
        } else {
            out[pos++] = *p++;
        }
    }
    out[pos] = '\0';
    return out;
}

static char *resolve_template(const char *base, const char *file) {
    char *path = malloc(strlen(base) + strlen(file) + 2);
    if (!path) die("malloc");
    sprintf(path, "%s/%s", base, file);
    return path;
}

int main(int argc, char *argv[]) {
    const char *spec_path = NULL;
    const char *out_path = NULL;
    const char *base_dir = NULL;
    int check_mode = 0;
    const char *check_spec = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: spec2c [--base <dir>] [-o <out.c>] <spec.json>\n"
                "       spec2c --check <file.c> [--spec <spec.json>] [--base <dir>]\n"
                "\n"
                "  <spec.json>     JSON tool specification (use '-' for stdin)\n"
                "  -o <out.c>      Write output to file (default: stdout)\n"
                "  --base <dir>    Template/skeleton base directory\n"
                "  --check <file>  Run conformance check on generated C file\n"
                "  --spec <spec>   Spec file for scaffold comparison (with --check)\n"
                "  --help, -h\n");
            return 0;
        } else if (strcmp(argv[i], "--check") == 0) {
            check_mode = 1;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                spec_path = argv[++i];
            } else {
                die("missing file argument for --check");
            }
        } else if (strcmp(argv[i], "--spec") == 0) {
            if (++i >= argc) die("missing argument for --spec");
            check_spec = argv[i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) die("missing argument for -o");
            out_path = argv[i];
        } else if (strcmp(argv[i], "--base") == 0) {
            if (++i >= argc) die("missing argument for --base");
            base_dir = argv[i];
        } else if (!spec_path) {
            spec_path = argv[i];
        } else {
            die("unexpected argument");
        }
    }

    if (check_mode) {
        if (!spec_path) die("file argument required for --check");
        if (!base_dir) base_dir = ".";
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "spec2c-check \"%s\" --base \"%s\" --patterns \"%s/soul-patterns.json\"%s%s%s",
            spec_path, base_dir, base_dir,
            check_spec ? " --spec \"" : "",
            check_spec ? check_spec : "",
            check_spec ? "\"" : "");
        return system(cmd);
    }

    if (!spec_path) die("spec file required");
    if (!base_dir) base_dir = ".";

    char *skel_path = resolve_template(base_dir, "skeleton.json");
    char *skel_text = read_file(skel_path);
    free(skel_path);

    cJSON *skel = cJSON_Parse(skel_text);
    free(skel_text);
    if (!skel) die("JSON parse error in skeleton.json");

    char *spec_text = read_file(spec_path);
    spec_t spec;
    parse_spec(spec_text, &spec);
    free(spec_text);

    subst_t subs[SUBST_MAX];
    int nsubs = 0;
    compute_substs(&spec, subs, &nsubs);

    int has_config = spec.nconfig_keys > 0;
    int has_db = spec.has_db;

    cJSON *sections = cJSON_GetObjectItemCaseSensitive(skel, "sections");
    if (!cJSON_IsArray(sections)) die("skeleton.json: missing \"sections\" array");

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) die("cannot open output file");
    }

    for (int i = 0; i < cJSON_GetArraySize(sections); i++) {
        cJSON *sec = cJSON_GetArrayItem(sections, i);
        if (!sec) continue;

        cJSON *cond = cJSON_GetObjectItemCaseSensitive(sec, "condition");
        const char *cond_str = cJSON_IsString(cond) ? cond->valuestring : "";
        int include = 0;
        if (strcmp(cond_str, "always") == 0) include = 1;
        else if (strcmp(cond_str, "has-config") == 0) include = has_config;
        else if (strcmp(cond_str, "has-db") == 0) include = has_db;
        if (!include) continue;

        cJSON *file = cJSON_GetObjectItemCaseSensitive(sec, "file");
        if (!cJSON_IsString(file)) continue;

        char *tpath = resolve_template(base_dir, file->valuestring);
        char *tmpl = read_file(tpath);
        free(tpath);

        char *result = subst_apply(tmpl, subs, nsubs);
        free(tmpl);

        fputs(result, out);
        free(result);
    }

    if (out != stdout) fclose(out);

    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", 1);
    cJSON_AddStringToObject(r, "output", out_path ? out_path : "(stdout)");
    char *js = cJSON_PrintUnformatted(r);
    printf("%s\n", js);
    free(js);
    cJSON_Delete(r);
    cJSON_Delete(skel);

    free((void *)spec.name);
    free((void *)spec.ident);
    free((void *)spec.description);
    free((void *)spec.core_function);
    free(spec.config_keys_str);

    return 0;
}
