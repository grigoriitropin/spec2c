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

static const char *jstr(const cJSON *obj, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v || !cJSON_IsString(v)) return NULL;
    return v->valuestring;
}

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

static void parse_spec_from_cjson(cJSON *root, spec_t *s) {
    memset(s, 0, sizeof(*s));

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

/* ── .ipm format support (Phase 1: template substitution from AST) ──────── */

typedef struct {
    cJSON *meta;        /* full parsed JSON */
    const char *name;
    const char *type;
    const char *desc;
} ipm_spec_t;

static void ipm_add_subst(subst_t *subs, int *n, const char *k, const char *v) {
    if (*n >= SUBST_MAX) die("too many substitutions");
    subs[*n].key = k;
    snprintf(subs[*n].val, VAL_SZ, "%s", v ? v : "");
    (*n)++;
}

/* ── Phase 2a: AST-to-C compiler ─────────────────────────────────────── */

static const char *vartype_to_c(const char *t) {
    if (!strcmp(t, "string")) return "char *";
    if (!strcmp(t, "int")) return "int";
    if (!strcmp(t, "float")) return "double";
    if (!strcmp(t, "boolean")) return "int";
    if (!strcmp(t, "json_object")) return "cJSON *";
    if (!strcmp(t, "json_array")) return "cJSON *";
    if (!strcmp(t, "db_handle")) return "struct vehir_db *";
    return "void *";
}

static void compile_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type) {
    if (!cJSON_IsArray(instructions)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(instructions); ii++) {
        cJSON *inst = cJSON_GetArrayItem(instructions, ii);
        if (!inst) continue;
        cJSON *it = cJSON_GetObjectItemCaseSensitive(inst, "instruction_type");
        if (!cJSON_IsString(it)) continue;
        const char *type = it->valuestring;
        for (int s = 0; s < indent; s++) fputs("  ", out);

        if (!strcmp(type, "variable_declaration")) {
            const char *vn = jstr(inst, "variable_name");
            const char *vt = jstr(inst, "variable_type");
            const char *op = jstr(inst, "assignment_operation");
            cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
            const char *src = "";
            if (cJSON_IsString(st)) src = st->valuestring;
            else if (st) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(st, "source"); if (cJSON_IsString(s2)) src = s2->valuestring; }
            if (vn && vt && op) {
                fprintf(out, "%s %s = %s(%s);\n", vartype_to_c(vt), vn, op, src);
            }
        } else if (!strcmp(type, "function_invocation")) {
            const char *fn = jstr(inst, "invocation_name");
            const char *rv = jstr(inst, "result_assignment_variable");
            if (fn) {
                if (rv) fprintf(out, "int %s = ", rv);
                fprintf(out, "%s(/* args */);\n", fn);
            }
        } else if (!strcmp(type, "conditional_branch")) {
            const char *op = jstr(inst, "condition_operation");
            cJSON *ct = cJSON_GetObjectItemCaseSensitive(inst, "condition_target");
            const char *cv = jstr(inst, "condition_value");
            const char *ck = jstr(inst, "condition_key");
            const char *tgt = "";
            if (cJSON_IsString(ct)) tgt = ct->valuestring;
            else if (ct) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(ct, "source"); if (cJSON_IsString(s2)) tgt = s2->valuestring; }
            if (!strcmp(op, "key_exists")) {
                fprintf(out, "if (cJSON_HasObjectItem(%s, \"%s\")) {\n", tgt, ck ? ck : "");
            } else if (!strcmp(op, "string_equals") || !strcmp(op, "enum_equals")) {
                fprintf(out, "if (strcmp(%s, \"%s\") == 0) {\n", tgt, cv ? cv : "");
            } else if (!strcmp(op, "is_not_null")) {
                fprintf(out, "if (%s != NULL) {\n", tgt);
            } else {
                fprintf(out, "if (/* %s */ 0) {\n", op);
            }
            cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
            compile_instructions(bon, out, indent + 1, return_type);
            fprintf(out, "%*c} else {\n", indent * 2, ' ');
            cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
            compile_instructions(bof, out, indent + 1, return_type);
            fprintf(out, "%*c}\n", indent * 2, ' ');
        } else if (!strcmp(type, "return_statement")) {
            cJSON *rp = cJSON_GetObjectItemCaseSensitive(inst, "return_payload");
            int is_void = return_type && !strcmp(return_type, "void");
            if (rp) {
                const char *es = jstr(rp, "execution_status");
                const char *ec = jstr(rp, "error_code");
                if (es && !strcmp(es, "failure")) {
                    fprintf(out, "die(\"%s\");%s\n", ec ? ec : "unknown error", is_void ? "" : " return 1;");
                } else if (is_void) {
                    fprintf(out, "return;\n");
                } else {
                    fprintf(out, "return 0;\n");
                }
            } else {
                fprintf(out, "%s\n", is_void ? "return;" : "return 0;");
            }
        } else if (!strcmp(type, "iterate_over_collection")) {
            const char *col = jstr(inst, "collection_variable");
            const char *item = jstr(inst, "item_variable");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
            if (col && item && body) {
                fprintf(out, "for (int _i_%s = 0; _i_%s < cJSON_GetArraySize(%s); _i_%s++) {\n",
                        col, col, col, col);
                fprintf(out, "  cJSON *%s = cJSON_GetArrayItem(%s, _i_%s);\n", item, col, col);
                compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "%*c}\n", indent * 2, ' ');
            }
        } else if (!strcmp(type, "iterate_over_object_keys")) {
            const char *col = jstr(inst, "collection_variable");
            const char *key  = jstr(inst, "key_variable");
            const char *val  = jstr(inst, "value_variable");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
            if (col && key && val && body) {
                fprintf(out, "cJSON *%s = %s;\n", val, col);
                fprintf(out, "cJSON_ArrayForEach(%s, %s) {\n", val, col);
                fprintf(out, "  const char *%s = %s->string;\n", key, val);
                compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "}\n");
            }
        } else if (!strcmp(type, "access_json_field")) {
            const char *vn = jstr(inst, "variable_name");
            const char *vt = jstr(inst, "variable_type");
            const char *so = jstr(inst, "source_object");
            const char *fn = jstr(inst, "field_name");
            if (vn && so && fn) {
                const char *ct = vartype_to_c(vt);
                fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");%s\n",
                        vn, so, fn, (!strcmp(ct, "int") || !strcmp(ct, "double")) ? "" : "");
            }
        } else if (!strcmp(type, "database_execute_parameterized")) {
            const char *sql = jstr(inst, "sql_query_string");
            fprintf(out, "/* DB exec: %s */\n", sql ? sql : "?");
        }
    }
}

static void compile_functions_to_c(const ipm_spec_t *spec, FILE *out) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;

    /* Inject standard C headers; cjson comes from template's {{compiler_includes}} */
    fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n\n");

    cJSON *fn = funcs->child;
    while (fn) {
        const char *name = fn->string;
        const char *desc = jstr(fn, "description");
        cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
        cJSON *body  = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");

        if (body && cJSON_IsArray(body)) {
            const char *ret = jstr(fn, "return_type");
            const char *ret_c = "void";
            if (ret) {
                if (!strcmp(ret, "void")) ret_c = "void";
                else if (!strcmp(ret, "int")) ret_c = "int";
                else if (!strcmp(ret, "double")) ret_c = "double";
                else if (!strcmp(ret, "boolean")) ret_c = "int";
                else if (!strcmp(ret, "string") || !strcmp(ret, "char")) ret_c = "char *";
                else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
                else if (!strcmp(ret, "json_array")) ret_c = "cJSON *";
                else if (!strcmp(ret, "db_handle")) ret_c = "struct vehir_db *";
            }
            fprintf(out, "/* %s: %s */\n", name, desc ? desc : "no description");
            fprintf(out, "static %s %s(", ret_c, name);
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    const char *pn = jstr(par, "parameter_name");
                    const char *pt = jstr(par, "parameter_type");
                    if (p > 0) fprintf(out, ", ");
                    fprintf(out, "%s %s", vartype_to_c(pt), pn);
                }
            }
            fprintf(out, ") {\n");
            compile_instructions(body, out, 1, ret_c);
            fprintf(out, "}\n\n");
        }
        fn = fn->next;
    }
}

/* ── Phase 1: template substitution ──────────────────────────────────── */

static void generate_from_ipm(const ipm_spec_t *spec, const char *out_path) {
    FILE *out_fp = out_path ? fopen(out_path, "w") : stdout;
    if (!out_fp) die("cannot open output file");

    /* Build substitution context first (needed by both passes) */
    subst_t subs[SUBST_MAX]; int nsubs = 0;
    ipm_add_subst(subs, &nsubs, "package_name", spec->name);
    ipm_add_subst(subs, &nsubs, "package_type", spec->type);
    ipm_add_subst(subs, &nsubs, "package_description", spec->desc ? spec->desc : "No description");

    cJSON *cfg = cJSON_GetObjectItemCaseSensitive(spec->meta, "configuration_keys");
    int has_config = cfg && cJSON_IsArray(cfg) && cJSON_GetArraySize(cfg) > 0;
    ipm_add_subst(subs, &nsubs, "has_configuration", has_config ? "1" : "0");

    ipm_add_subst(subs, &nsubs, "compiler_includes", "#include <cjson/cJSON.h>\n#include <string.h>");
    ipm_add_subst(subs, &nsubs, "compiler_function_implementations", "");
    ipm_add_subst(subs, &nsubs, "argument_parsing_logic", "if (argc < 2) print_usage_and_exit(argv[0]);");
    ipm_add_subst(subs, &nsubs, "pipeline_initialization", "fprintf(stderr, \"gen\\n\");");
    ipm_add_subst(subs, &nsubs, "pipeline_execution", "fprintf(stderr, \"done\\n\");");
    ipm_add_subst(subs, &nsubs, "ipm_metadata_header", "/* IPM Generated Code — DO NOT EDIT */");

    /* Emit includes from the first template (provides cJSON, etc.) */
    cJSON *templates = cJSON_GetObjectItemCaseSensitive(spec->meta, "template_definitions");
    if (templates && cJSON_IsObject(templates) && templates->child) {
        cJSON *first = templates->child;
        if (cJSON_IsString(first)) {
            char *inc = subst_apply("{{compiler_includes}}", subs, nsubs);
            fprintf(out_fp, "%s\n", inc);
            free(inc);
        }
    }

    /* Phase 2a: compile AST functions to C */
    compile_functions_to_c(spec, out_fp);

    /* Phase 1: template substitution for remaining templates */
    if (!templates || !cJSON_IsObject(templates)) return;
    cJSON *tmpl = templates->child;

    while (tmpl) {
        const char *tmpl_name = tmpl->string;
        if (cJSON_IsString(tmpl)) {
            char *gen = subst_apply(tmpl->valuestring, subs, nsubs);
            fprintf(out_fp, "/* --- template: %s --- */\n", tmpl_name);
            fprintf(out_fp, "%s\n", gen);
            free(gen);
        }
        tmpl = tmpl->next;
    }

    if (out_path) fclose(out_fp);
}

/* ── old .spec.json format (backward compat) ─────────────────────────── */

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

    /* Read and parse the spec first to detect .ipm format */
    char *spec_text = read_file(spec_path);
    cJSON *spec_json = cJSON_Parse(spec_text);
    if (!spec_json) die("JSON parse error in spec file");

    /* Detect .ipm format by presence of package_name key */
    cJSON *pkg_name = cJSON_GetObjectItemCaseSensitive(spec_json, "package_name");
    if (pkg_name && cJSON_IsString(pkg_name)) {
        /* .ipm format — Phase 1: template substitution from AST */
        ipm_spec_t ipm;
        ipm.meta = spec_json;
        ipm.name = pkg_name->valuestring;
        cJSON *tp = cJSON_GetObjectItemCaseSensitive(spec_json, "package_type");
        ipm.type = (tp && cJSON_IsString(tp)) ? tp->valuestring : "tool";
        ipm.desc = "generated by spec2c from .ipm specification";

        generate_from_ipm(&ipm, out_path);

        cJSON *r = cJSON_CreateObject();
        cJSON_AddBoolToObject(r, "ok", 1);
        cJSON_AddStringToObject(r, "output", out_path ? out_path : "(stdout)");
        char *js = cJSON_PrintUnformatted(r);
        printf("%s\n", js);
        free(js);
        cJSON_Delete(r);
        cJSON_Delete(spec_json);
        free(spec_text);
        return 0;
    }

    /* Old .spec.json format — needs skeleton.json */
    char *skel_path = resolve_template(base_dir, "skeleton.json");
    char *skel_text = read_file(skel_path);
    free(skel_path);

    cJSON *skel = cJSON_Parse(skel_text);
    free(skel_text);
    if (!skel) die("JSON parse error in skeleton.json");

    /* Old .spec.json format — backward compat */
    spec_t spec;
    parse_spec_from_cjson(spec_json, &spec);
    /* parse_spec_from_cjson calls cJSON_Delete on spec_json — don't double-free */
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
