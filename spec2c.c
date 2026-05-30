// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)
//
// spec2c — JSON Spec → Vehir-pattern C Tool generator
//
// Reads a JSON specification and emits a correct-by-construction C skeleton
// that follows the Vehir coding laws: fail-hard, JSON output, broker config,
// bind-params DB, --help, no hardcode.

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cjson/cJSON.h>

typedef struct {
    const char *name;
    const char *ident;
    const char *description;
    char      **config_keys;
    int         nconfig_keys;
    int         has_db;
    const char *core_file;
    const char *core_function;
    int         ncommands;
} spec_t;

static char *name_to_ident(const char *name) {
    char *ident = strdup(name);
    if (!ident) return NULL;
    for (char *p = ident; *p; p++)
        if (*p == '-') *p = '_';
    return ident;
}

static _Noreturn void die(const char *msg) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "spec2c: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", msg);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else { printf("{\"ok\":false,\"error\":\"%s\"}\n", msg); }
    cJSON_Delete(root);
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
}

static _Noreturn void die_detail(const char *msg, const char *detail) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "spec2c: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", msg);
    if (detail) cJSON_AddStringToObject(root, "detail", detail);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else { printf("{\"ok\":false,\"error\":\"%s\"}\n", msg); }
    cJSON_Delete(root);
    fprintf(stderr, "spec2c: %s", msg);
    if (detail) fprintf(stderr, ": %s", detail);
    fprintf(stderr, "\n");
    exit(1);
}

static char *read_file(const char *path) {
    FILE *f;
    if (strcmp(path, "-") == 0) {
        f = stdin;
    } else {
        f = fopen(path, "r");
        if (!f) die_detail("cannot open spec file", strerror(errno));
    }
    size_t cap = 8192;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) die("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); die("realloc failed"); }
            buf = tmp;
        }
    }
    if (ferror(f)) { free(buf); die_detail("read error", strerror(errno)); }
    buf[len] = '\0';
    if (f != stdin) fclose(f);
    return buf;
}

static void parse_spec(const char *text, spec_t *spec) {
    memset(spec, 0, sizeof(*spec));

    cJSON *root = cJSON_Parse(text);
    if (!root) {
        const char *ep = cJSON_GetErrorPtr();
        char detail[256];
        snprintf(detail, sizeof(detail), "near: %.80s", ep ? ep : "(unknown)");
        die_detail("JSON parse error in spec", detail);
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!name || !cJSON_IsString(name) || !name->valuestring[0])
        die("spec missing required field: \"name\" (non-empty string)");
    spec->name = strdup(name->valuestring);
    if (!spec->name) die("strdup failed");
    spec->ident = name_to_ident(spec->name);
    if (!spec->ident) die("strdup failed");

    cJSON *desc = cJSON_GetObjectItemCaseSensitive(root, "description");
    if (desc && cJSON_IsString(desc) && desc->valuestring[0])
        spec->description = strdup(desc->valuestring);
    else
        spec->description = strdup("No description");
    if (!spec->description) die("strdup failed");

    cJSON *commands = cJSON_GetObjectItemCaseSensitive(root, "commands");
    if (commands && cJSON_IsObject(commands))
        spec->ncommands = cJSON_GetArraySize(commands);

    cJSON *ckeys = cJSON_GetObjectItemCaseSensitive(root, "config_keys");
    if (ckeys && cJSON_IsArray(ckeys)) {
        spec->nconfig_keys = cJSON_GetArraySize(ckeys);
        if (spec->nconfig_keys > 0) {
            spec->config_keys = calloc((size_t)spec->nconfig_keys, sizeof(char *));
            if (!spec->config_keys) die("calloc failed");
            for (int i = 0; i < spec->nconfig_keys; i++) {
                cJSON *key = cJSON_GetArrayItem(ckeys, i);
                if (!cJSON_IsString(key) || !key->valuestring[0])
                    die("config_keys array must contain non-empty strings");
                spec->config_keys[i] = strdup(key->valuestring);
                if (!spec->config_keys[i]) die("strdup failed");
            }
        }
    }

    cJSON *dbq = cJSON_GetObjectItemCaseSensitive(root, "db_queries");
    if (dbq && cJSON_IsArray(dbq) && cJSON_GetArraySize(dbq) > 0)
        spec->has_db = 1;

    cJSON *core = cJSON_GetObjectItemCaseSensitive(root, "core");
    if (core && cJSON_IsObject(core)) {
        cJSON *cf = cJSON_GetObjectItemCaseSensitive(core, "file");
        if (cf && cJSON_IsString(cf) && cf->valuestring[0])
            spec->core_file = strdup(cf->valuestring);

        cJSON *cfn = cJSON_GetObjectItemCaseSensitive(core, "function");
        if (cfn && cJSON_IsString(cfn) && cfn->valuestring[0])
            spec->core_function = strdup(cfn->valuestring);
    }

    if (!spec->core_file) spec->core_file = strdup("tool_core.c");
    if (!spec->core_file) die("strdup failed");

    if (!spec->core_function) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s_run", spec->ident);
        spec->core_function = strdup(buf);
    }
    if (!spec->core_function) die("strdup failed");

    cJSON_Delete(root);
}

static void spec_free(spec_t *spec) {
    free((void *)spec->name);
    free((void *)spec->ident);
    free((void *)spec->description);
    for (int i = 0; i < spec->nconfig_keys; i++)
        free(spec->config_keys[i]);
    free(spec->config_keys);
    free((void *)spec->core_file);
    free((void *)spec->core_function);
}

static void emit_header(const spec_t *spec, FILE *out) {
    fprintf(out,
        "// Generated by spec2c from %s.spec.json. DO NOT EDIT.\n"
        "// SPDX-License-Identifier: Apache-2.0\n"
        "#define _POSIX_C_SOURCE 200809L\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n"
        "#include <errno.h>\n"
        "#include <cjson/cJSON.h>\n",
        spec->name);

    if (spec->has_db)
        fprintf(out, "#include \"db.h\"\n");

    if (spec->nconfig_keys > 0)
        fprintf(out, "#include <sys/stat.h>\n");

    fprintf(out, "\n");
}

static void emit_die(const spec_t *spec, FILE *out) {
    fprintf(out,
        "static _Noreturn void die_json(const char *error) {\n"
        "    cJSON *root = cJSON_CreateObject();\n"
        "    if (!root) {\n"
        "        fprintf(stderr, \"%s: FATAL: cJSON alloc failed\\n\");\n"
        "        printf(\"{\\\"ok\\\":false,\\\"error\\\":\\\"alloc failed\\\"}\\n\");\n"
        "        exit(1);\n"
        "    }\n"
        "    cJSON_AddBoolToObject(root, \"ok\", 0);\n"
        "    cJSON_AddStringToObject(root, \"error\", error);\n"
        "    char *s = cJSON_PrintUnformatted(root);\n"
        "    if (s) { printf(\"%%s\\n\", s); free(s); }\n"
        "    else { printf(\"{\\\"ok\\\":false,\\\"error\\\":\\\"%%s\\\"}\\n\", error); }\n"
        "    cJSON_Delete(root);\n"
        "    fprintf(stderr, \"%s: %%s\\n\", error);\n"
        "    exit(1);\n"
        "}\n\n",
        spec->name, spec->name);
}

static void emit_config(const spec_t *spec, FILE *out) {
    fprintf(out,
        "static char *default_config_path(void) {\n"
        "    const char *home = getenv(\"HOME\");\n"
        "    if (!home || !home[0]) return NULL;\n"
        "    char *path = malloc(strlen(home) + 32);\n"
        "    if (!path) return NULL;\n"
        "    sprintf(path, \"%%s/.config/vehir/env\", home);\n"
        "    return path;\n"
        "}\n\n");

    fprintf(out,
        "static char *cfg_load_value(const char *path, const char *key) {\n"
        "    struct stat st;\n"
        "    if (stat(path, &st) != 0) return NULL;\n"
        "    if (st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {\n"
        "        fprintf(stderr, \"%s: config %%s is readable by group/other (mode %%04o)\\n\",\n"
        "                path, st.st_mode & 0777);\n"
        "        return NULL;\n"
        "    }\n"
        "    FILE *f = fopen(path, \"r\");\n"
        "    if (!f) return NULL;\n"
        "    size_t keylen = strlen(key);\n"
        "    char line[2048];\n"
        "    char *result = NULL;\n"
        "    while (fgets(line, (int)sizeof(line), f)) {\n"
        "        char *p = line;\n"
        "        while (*p == ' ' || *p == '\\t') p++;\n"
        "        if (*p == '#' || *p == '\\n' || *p == '\\0') continue;\n"
        "        if (strncmp(p, key, keylen) != 0) continue;\n"
        "        char *eq = p + keylen;\n"
        "        while (*eq == ' ' || *eq == '\\t') eq++;\n"
        "        if (*eq != '=') continue;\n"
        "        char *val = eq + 1;\n"
        "        size_t vlen = strlen(val);\n"
        "        while (vlen > 0 && (val[vlen-1] == '\\n' || val[vlen-1] == '\\r' ||\n"
        "               val[vlen-1] == ' ' || val[vlen-1] == '\\t')) val[--vlen] = '\\0';\n"
        "        result = strdup(val);\n"
        "        break;\n"
        "    }\n"
        "    fclose(f);\n"
        "    return result;\n"
        "}\n\n",
        spec->name);

    fprintf(out,
        "static char *cfg_require(const char *path, const char *key) {\n"
        "    char *val = cfg_load_value(path, key);\n"
        "    if (!val) {\n"
        "        char msg[384];\n"
        "        snprintf(msg, sizeof(msg), \"key %%s not found in config %%s\", key, path);\n"
        "        die_json(msg);\n"
        "    }\n"
        "    return val;\n"
        "}\n\n");
}

static void emit_db_helpers(const spec_t *spec, FILE *out) {
    (void)spec;
    fprintf(out,
        "static void db_check(vehir_db *db) {\n"
        "    if (!db || vehir_db_error(db)[0]) {\n"
        "        char msg[256];\n"
        "        snprintf(msg, sizeof(msg), \"DB error: %%s\",\n"
        "                 db ? vehir_db_error(db) : \"null handle\");\n"
        "        die_json(msg);\n"
        "    }\n"
        "}\n\n");

    fprintf(out,
        "static cJSON *db_query_json(vehir_db *db, const char *sql,\n"
        "                             const char *const *params, int nparams) {\n"
        "    vehir_db_result *r = vehir_db_query(db, sql, params, nparams);\n"
        "    if (!r) { die_json(\"vehir_db_query returned NULL\"); }\n"
        "    if (vehir_db_result_error(r)[0]) {\n"
        "        char msg[384];\n"
        "        snprintf(msg, sizeof(msg), \"query error: %%s\",\n"
        "                 vehir_db_result_error(r));\n"
        "        vehir_db_result_free(r);\n"
        "        die_json(msg);\n"
        "    }\n"
        "    int nrows = vehir_db_result_nrows(r);\n"
        "    int ncols = vehir_db_result_ncols(r);\n"
        "    cJSON *out = cJSON_CreateObject();\n"
        "    if (!out) { vehir_db_result_free(r); die_json(\"cJSON alloc failed\"); }\n"
        "    cJSON_AddNumberToObject(out, \"nrows\", nrows);\n"
        "    cJSON_AddNumberToObject(out, \"ncols\", ncols);\n"
        "    cJSON *cols = cJSON_CreateArray();\n"
        "    if (!cols) { cJSON_Delete(out); vehir_db_result_free(r); die_json(\"cJSON alloc failed\"); }\n"
        "    for (int c = 0; c < ncols; c++)\n"
        "        cJSON_AddItemToArray(cols,\n"
        "            cJSON_CreateString(vehir_db_result_colname(r, c)));\n"
        "    cJSON_AddItemToObject(out, \"columns\", cols);\n"
        "    cJSON *rows = cJSON_CreateArray();\n"
        "    if (!rows) { cJSON_Delete(out); vehir_db_result_free(r); die_json(\"cJSON alloc failed\"); }\n"
        "    for (int i = 0; i < nrows; i++) {\n"
        "        cJSON *row = cJSON_CreateArray();\n"
        "        if (!row) { cJSON_Delete(out); vehir_db_result_free(r); die_json(\"cJSON alloc failed\"); }\n"
        "        for (int j = 0; j < ncols; j++) {\n"
        "            if (vehir_db_result_isnull(r, i, j))\n"
        "                cJSON_AddItemToArray(row, cJSON_CreateNull());\n"
        "            else\n"
        "                cJSON_AddItemToArray(row,\n"
        "                    cJSON_CreateString(vehir_db_result_value(r, i, j)));\n"
        "        }\n"
        "        cJSON_AddItemToArray(rows, row);\n"
        "    }\n"
        "    cJSON_AddItemToObject(out, \"rows\", rows);\n"
        "    vehir_db_result_free(r);\n"
        "    return out;\n"
        "}\n\n");
}

static void emit_core_decl(const spec_t *spec, FILE *out) {
    fprintf(out,
        "typedef struct vehir_db vehir_db;\n"
        "\n");

    if (spec->nconfig_keys > 0 && spec->ncommands > 0) {
        fprintf(out,
            "extern int %s(vehir_db *db, int argc, char **argv, int arg_off,\n"
            "               const char *config_path);\n\n",
            spec->core_function);
    } else if (spec->nconfig_keys > 0) {
        fprintf(out,
            "extern int %s(vehir_db *db, int argc, char **argv,\n"
            "               const char *config_path);\n\n",
            spec->core_function);
    } else if (spec->ncommands > 0) {
        fprintf(out,
            "extern int %s(vehir_db *db, int argc, char **argv, int arg_off);\n\n",
            spec->core_function);
    } else {
        fprintf(out,
            "extern int %s(vehir_db *db, int argc, char **argv);\n\n",
            spec->core_function);
    }
}

static void emit_usage(const spec_t *spec, FILE *out) {
    fputs("static _Noreturn void usage(const char *prog) {\n"
          "    fprintf(stderr,\n"
          "        \"Usage: %s", out);

    if (spec->nconfig_keys > 0)
        fputs(" [--config <path>]", out);

    fputs("\\n\"\n", out);

    if (spec->description) {
        fputs("        \"\\n\"\n", out);
        fprintf(out, "        \"%s\\n\"\n", spec->description);
    }

    if (spec->nconfig_keys > 0) {
        fputs("        \"\\n\"\n"
              "        \"Config: ~/.config/vehir/env (override with --config <path>)\\n\"\n"
              "        \"  Required keys:", out);
        for (int i = 0; i < spec->nconfig_keys; i++)
            fprintf(out, " %s", spec->config_keys[i]);
        fputs("\\n\"\n"
              "        \"  File must be chmod 600 (owner-only). Tokens never touch process env.\\n\"\n", out);
    }

    fputs("        \"\\n\"\n"
          "        \"Output: JSON {\\\"ok\\\":true/false, ...}\\n\"\n"
          "        \"Exit:   0 = success, 1 = error, 2 = usage error\\n\",\n"
          "        prog);\n"
          "    exit(2);\n"
          "}\n\n", out);
}

static void emit_main(const spec_t *spec, FILE *out) {
    fprintf(out,
        "int main(int argc, char *argv[]) {\n"
        "    if (argc > 1 && (strcmp(argv[1], \"--help\") == 0 ||"
        " strcmp(argv[1], \"-h\") == 0))\n"
        "        usage(argv[0]);\n"
        "\n");

    if (spec->nconfig_keys > 0) {
        fprintf(out,
        "    char *config_path = NULL;\n"
        "    int arg_off = 1;\n"
        "    if (argc > 2 && strcmp(argv[1], \"--config\") == 0) {\n"
        "        config_path = strdup(argv[2]);\n"
        "        if (!config_path) die_json(\"strdup failed\");\n"
        "        arg_off = 3;\n"
        "    } else {\n"
        "        config_path = default_config_path();\n"
        "        if (!config_path) die_json(\"cannot determine config path (HOME unset?)\");\n"
        "    }\n"
        "\n");
    }

    if (spec->has_db) {
        fprintf(out,
        "    vehir_db *db = vehir_db_open();\n"
        "    db_check(db);\n"
        "\n");
    } else {
        fprintf(out,
        "    vehir_db *db = NULL;\n"
        "\n");
    }

    fprintf(out,
        "    int rc = %s(db, argc, argv",
        spec->core_function);

    if (spec->nconfig_keys > 0)
        fprintf(out, ", arg_off, config_path");
    else if (spec->ncommands > 0)
        fprintf(out, ", 1");

    fprintf(out, ");\n\n");

    if (spec->nconfig_keys > 0)
        fprintf(out, "    free(config_path);\n");

    if (spec->has_db)
        fprintf(out, "    vehir_db_close(db);\n");

    fprintf(out,
        "\n"
        "    return rc;\n"
        "}\n");
}

static void generate(const spec_t *spec, FILE *out) {
    emit_header(spec, out);
    emit_die(spec, out);
    if (spec->nconfig_keys > 0)
        emit_config(spec, out);
    if (spec->has_db)
        emit_db_helpers(spec, out);
    emit_core_decl(spec, out);
    emit_usage(spec, out);
    emit_main(spec, out);
}

static void print_ok(const char *output_path) {
    cJSON *root = cJSON_CreateObject();
    if (!root) die("cJSON alloc failed");
    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddStringToObject(root, "output", output_path ? output_path : "(stdout)");
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else { printf("{\"ok\":true,\"output\":\"%s\"}\n", output_path ? output_path : "(stdout)"); }
    cJSON_Delete(root);
}

int main(int argc, char *argv[]) {
    const char *spec_path = NULL;
    const char *out_path = NULL;
    int show_help = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            show_help = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) die("missing argument for -o");
            out_path = argv[++i];
        } else if (!spec_path) {
            spec_path = argv[i];
        } else {
            die_detail("unexpected argument", argv[i]);
        }
    }

    if (show_help || argc < 2) {
        fprintf(stderr,
            "Usage: spec2c [options] <spec.json>\n"
            "\n"
            "Generate a Vehir-pattern C skeleton from a JSON specification.\n"
            "\n"
            "  spec.json        JSON specification file (use '-' for stdin)\n"
            "  -o <output.c>    Write skeleton to file (default: stdout)\n"
            "  --help, -h       Show this help\n"
            "\n"
            "Output: JSON {\"ok\":true/false, ...}\n"
            "Exit:   0 = success, 1 = error\n");
        return show_help ? 0 : 1;
    }

    if (!spec_path) die("spec file required (use '-' for stdin)");

    char *text = read_file(spec_path);

    spec_t spec;
    parse_spec(text, &spec);
    free(text);

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) {
            spec_free(&spec);
            die_detail("cannot open output file", strerror(errno));
        }
    }

    generate(&spec, out);

    if (out != stdout) {
        if (ferror(out)) {
            fclose(out);
            spec_free(&spec);
            die_detail("write error", strerror(errno));
        }
        fclose(out);
    }

    print_ok(out_path);
    spec_free(&spec);
    return 0;
}
