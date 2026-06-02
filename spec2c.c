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
#include <dirent.h>
#include <sys/stat.h>
#include "ipm_builtins.h"

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
    if (!v || !cJSON_IsString(v)) return "";
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
            const char *rt = jstr(inst, "result_type"); /* optional type hint */
            cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
            if (fn[0]) {
                if (rv[0]) {
                    const char *rc = "int";
                    if (rt) {
                        if (!strcmp(rt, "string") || !strcmp(rt, "char")) rc = "char *";
                        else if (!strcmp(rt, "json_object")) rc = "cJSON *";
                        else if (!strcmp(rt, "json_array")) rc = "cJSON *";
                        else if (!strcmp(rt, "subst_table")) rc = "subst_table *";
                        else if (!strcmp(rt, "string_buffer")) rc = "string_buffer *";
                        else if (!strcmp(rt, "int")) rc = "int";
                        else if (!strcmp(rt, "void")) rc = "void";
                    } else {
                        if (strstr(fn, "read_file")) rc = "char *";
                        else if (strstr(fn, "parse_json")) rc = "cJSON *";
                        else if (strstr(fn, "create_hash_table")) rc = "subst_table *";
                        else if (strstr(fn, "hash_table_lookup")) rc = "const char *";
                        else if (!strcmp(fn, "string_substitute")) rc = "char *";
                        else if (!strcmp(fn, "compute_substs")) rc = "subst_table *";
                        else if (!strcmp(fn, "generate_from_ipm")) rc = "int";
                    }
                    fprintf(out, "%s %s = ", rc, rv);
                }
                fprintf(out, "%s(", fn);
                /* emit arguments — quoted if starts with \" (literal), unquoted if variable */
                if (args && cJSON_IsObject(args)) {
                    cJSON *arg = args->child; int first = 1;
                    while (arg) {
                        if (!first) fprintf(out, ", ");
                        if (cJSON_IsString(arg)) {
                            const char *sv = arg->valuestring;
                            if (sv[0] == '"') fprintf(out, "%s", sv);  /* literal — already quoted */
                            else fprintf(out, "%s", sv);                /* variable reference */
                        } else if (cJSON_IsNumber(arg)) fprintf(out, "\"%d\"", arg->valueint);  /* stringify numbers */
                        first = 0;
                        arg = arg->next;
                    }
                }
                fprintf(out, ");\n");
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
                const char *rv = jstr(rp, "value");
                if (es && !strcmp(es, "failure")) {
                    fprintf(out, "die(\"%s\");%s\n", ec[0] ? ec : "unknown error", is_void ? "" : " return 1;");
                } else if (rv[0]) {
                    fprintf(out, "return %s;\n", rv);
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
            if (col[0] && item[0] && body) {
                fprintf(out, "for (int _i_%s = 0; _i_%s < cJSON_GetArraySize(%s); _i_%s++) {\n",
                        col, col, col, col);
                fprintf(out, "  cJSON *%s = cJSON_GetArrayItem(%s, _i_%s);\n", item, col, col);
                fprintf(out, "  const char *%s_valstr = %s ? %s->valuestring : \"\";\n", item, item, item);
                compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "%*c}\n", indent * 2, ' ');
            }
        } else if (!strcmp(type, "iterate_over_object_keys")) {
            const char *col = jstr(inst, "collection_variable");
            const char *key  = jstr(inst, "key_variable");
            const char *val  = jstr(inst, "value_variable");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
            if (col[0] && val[0] && body) {
                fprintf(out, "cJSON *%s = %s;\n", val, col);
                fprintf(out, "cJSON_ArrayForEach(%s, %s) {\n", val, col);
                if (key) fprintf(out, "  const char *%s = %s->string;\n", key, val);
                fprintf(out, "  const char *%s_valstr = %s->valuestring;\n", val, val);
                compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "}\n");
            }
        } else if (!strcmp(type, "access_json_field")) {
            const char *vn = jstr(inst, "variable_name");
            const char *vt = jstr(inst, "variable_type");
            const char *so = jstr(inst, "source_object");
            const char *fn = jstr(inst, "field_name");
            if (vn[0] && so[0] && fn[0]) {
                if (vt && (!strcmp(vt, "string") || !strcmp(vt, "char"))) {
                    /* string field — extract value directly */
                    fprintf(out, "const char *%s = cJSON_GetObjectItemCaseSensitive(%s,\"%s\") ? cJSON_GetObjectItemCaseSensitive(%s,\"%s\")->valuestring : \"\";\n",
                            vn, so, fn, so, fn);
                } else if (vt && (!strcmp(vt, "json_object") || !strcmp(vt, "json_array"))) {
                    fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
                } else {
                    fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
                }
            }
        } else if (!strcmp(type, "database_execute_parameterized")) {
            const char *sql = jstr(inst, "sql_query_string");
            fprintf(out, "/* DB exec: %s */\n", sql ? sql : "?");
        }
    }
}

static void compile_functions_to_c(const ipm_spec_t *spec, FILE *out, int is_library) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn;

    /* Include module header */
    const char *modname = jstr(spec->meta, "module_name");
    if (modname[0]) {
        fprintf(out, "#include \"%s.h\"\n\n", modname);
    } else {
        fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n#include \"ipm_builtins.h\"\n\n");
        /* Forward-declare for monolithic (no-header) mode */
        fn = funcs->child;
        while (fn) {
            const char *name = fn->string;
            const char *ret = jstr(fn, "return_type");
            const char *ret_c = "void";
            if (ret[0]) {
                if (!strcmp(ret, "void")) ret_c = "void";
                else if (!strcmp(ret, "int")) ret_c = "int";
                else if (!strcmp(ret, "double")) ret_c = "double";
                else if (!strcmp(ret, "boolean")) ret_c = "int";
                else if (!strcmp(ret, "string") || !strcmp(ret, "char")) ret_c = "char *";
                else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
                else if (!strcmp(ret, "json_array")) ret_c = "cJSON *";
                else if (!strcmp(ret, "db_handle")) ret_c = "struct vehir_db *";
                else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
            }
            fprintf(out, "%s%s %s(", is_library ? "" : "static ", ret_c, name);
            cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    const char *pn = jstr(par, "parameter_name");
                    const char *pt = jstr(par, "parameter_type");
                    if (p > 0) fprintf(out, ", ");
                    fprintf(out, "%s %s", vartype_to_c(pt), pn);
                }
            }
            fprintf(out, ");\n");
            fn = fn->next;
        }
        fprintf(out, "\n");
    }

    /* Generate function bodies */
    fn = funcs->child;
    while (fn) {
        const char *name = fn->string;
        const char *desc = jstr(fn, "description");
        cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
        cJSON *body  = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");

        if (body && cJSON_IsArray(body)) {
            const char *ret = jstr(fn, "return_type");
            const char *ret_c = "void";
            if (ret[0]) {
                if (!strcmp(ret, "void")) ret_c = "void";
                else if (!strcmp(ret, "int")) ret_c = "int";
                else if (!strcmp(ret, "double")) ret_c = "double";
                else if (!strcmp(ret, "boolean")) ret_c = "int";
                else if (!strcmp(ret, "string") || !strcmp(ret, "char")) ret_c = "char *";
                else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
                else if (!strcmp(ret, "json_array")) ret_c = "cJSON *";
                else if (!strcmp(ret, "db_handle")) ret_c = "struct vehir_db *";
                else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
            }
            fprintf(out, "/* %s: %s */\n", name, desc[0] ? desc : "no description");
            fprintf(out, "%s%s %s(", is_library ? "" : (modname[0] ? "" : "static "), ret_c, name);
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

static void generate_header(const ipm_spec_t *spec, const char *hdr_path) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    const char *modname = jstr(spec->meta, "module_name");
    if (!modname[0] || !hdr_path) return;

    FILE *hdr = fopen(hdr_path, "w");
    if (!hdr) die("cannot open header file");

    char guard[256];
    snprintf(guard, sizeof(guard), "%s_H", modname);
    for (char *p = guard; *p; p++) if (*p == '-') *p = '_';

    fprintf(hdr, "/* Auto-generated from %s.ipm */\n", modname);
    fprintf(hdr, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(hdr, "#include \"ipm_builtins.h\"\n");

    cJSON *imports = cJSON_GetObjectItemCaseSensitive(spec->meta, "imports");
    if (imports && cJSON_IsArray(imports)) {
        for (int i = 0; i < cJSON_GetArraySize(imports); i++) {
            cJSON *imp = cJSON_GetArrayItem(imports, i);
            if (cJSON_IsString(imp))
                fprintf(hdr, "#include \"%s.h\"\n", imp->valuestring);
        }
    }
    fprintf(hdr, "\n");

    if (funcs && cJSON_IsObject(funcs)) {
        cJSON *fn = funcs->child;
        while (fn) {
            const char *name = fn->string;
            const char *ret = jstr(fn, "return_type");
            const char *ret_c = "void";
            if (ret[0]) {
                if (!strcmp(ret, "void")) ret_c = "void";
                else if (!strcmp(ret, "int")) ret_c = "int";
                else if (!strcmp(ret, "string") || !strcmp(ret, "char")) ret_c = "char *";
                else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
                else if (!strcmp(ret, "json_array")) ret_c = "cJSON *";
                else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
                else if (!strcmp(ret, "string_buffer")) ret_c = "string_buffer *";
            }
            fprintf(hdr, "%s %s(", ret_c, name);
            cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    const char *pn = jstr(par, "parameter_name");
                    const char *pt = jstr(par, "parameter_type");
                    if (p > 0) fprintf(hdr, ", ");
                    fprintf(hdr, "%s %s", vartype_to_c(pt), pn);
                }
            }
            fprintf(hdr, ");\n");
            fn = fn->next;
        }
    }

    fprintf(hdr, "\n#endif /* %s_H */\n", guard);
    fclose(hdr);
}

static void generate_from_ipm(const ipm_spec_t *spec, const char *out_path, int is_library) {
    /* For library mode: generate .h with all function prototypes */
    if (is_library && out_path) {
        char hdr_path[4096];
        size_t olen = strlen(out_path);
        if (olen > 2 && !strcmp(out_path + olen - 2, ".c")) {
            memcpy(hdr_path, out_path, olen - 2);
            hdr_path[olen - 2] = '.';
            hdr_path[olen - 1] = 'h';
            hdr_path[olen] = 0;
        } else {
            snprintf(hdr_path, sizeof(hdr_path), "%s.h", out_path);
        }
        FILE *hdr = fopen(hdr_path, "w");
        if (hdr) {
            fprintf(hdr, "/* Auto-generated library header */\n");
            fprintf(hdr, "#ifndef IPM_LIB_H\n#define IPM_LIB_H\n\n");
            fprintf(hdr, "#include \"ipm_builtins.h\"\n\n");
            cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
            if (funcs && cJSON_IsObject(funcs)) {
                cJSON *fn = funcs->child;
                while (fn) {
                    const char *name = fn->string;
                    const char *ret = jstr(fn, "return_type");
                    const char *ret_c = vartype_to_c(ret);
                    fprintf(hdr, "%s %s(", ret_c, name);
                    cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
                    if (params && cJSON_IsArray(params)) {
                        for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                            cJSON *par = cJSON_GetArrayItem(params, p);
                            if (p > 0) fprintf(hdr, ", ");
                            fprintf(hdr, "%s %s", vartype_to_c(jstr(par, "parameter_type")), jstr(par, "parameter_name"));
                        }
                    }
                    fprintf(hdr, ");\n");
                    fn = fn->next;
                }
            }
            fprintf(hdr, "\n#endif /* IPM_LIB_H */\n");
            fclose(hdr);
        }
    }

    /* Generate header if module_name is present and output is .c */
    const char *modname = jstr(spec->meta, "module_name");
    if (modname[0] && out_path && strcmp(out_path, "/dev/null")) {
        char hdr_path[4096];
        /* Derive header path from module_name + directory of output */
        const char *slash = strrchr(out_path, '/');
        int dirlen = slash ? (int)(slash - out_path + 1) : 0;
        snprintf(hdr_path, sizeof(hdr_path), "%.*s%s.h", dirlen, out_path, modname);
        generate_header(spec, hdr_path);
    }

    FILE *out_fp = out_path ? fopen(out_path, "w") : stdout;
    if (!out_fp) die("cannot open output file");

    /* For module mode, emit includes (header handles the rest) */
    if (modname[0]) {
        fprintf(out_fp, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n");
        fprintf(out_fp, "#include \"%s.h\"\n\n", modname);
    }

    /* Build substitution context first (needed by both passes) */
    subst_t subs[SUBST_MAX]; int nsubs = 0;
    ipm_add_subst(subs, &nsubs, "package_name", spec->name);
    ipm_add_subst(subs, &nsubs, "package_type", spec->type);
    ipm_add_subst(subs, &nsubs, "package_description", spec->desc ? spec->desc : "No description");

    cJSON *cfg = cJSON_GetObjectItemCaseSensitive(spec->meta, "configuration_keys");
    int has_config = cfg && cJSON_IsArray(cfg) && cJSON_GetArraySize(cfg) > 0;
    ipm_add_subst(subs, &nsubs, "has_configuration", has_config ? "1" : "0");

    ipm_add_subst(subs, &nsubs, "compiler_includes", "");  /* compile_functions_to_c handles includes */
    ipm_add_subst(subs, &nsubs, "compiler_function_implementations", "");
    ipm_add_subst(subs, &nsubs, "argument_parsing_logic", "if (argc < 2) print_usage_and_exit(argv[0]);");
    ipm_add_subst(subs, &nsubs, "pipeline_initialization", "fprintf(stderr, \"gen\\n\");");
    ipm_add_subst(subs, &nsubs, "pipeline_execution", "fprintf(stderr, \"done\\n\");");
    ipm_add_subst(subs, &nsubs, "ipm_metadata_header", "/* IPM Generated Code — DO NOT EDIT */");

    /* Emit includes from the first template (provides cJSON, etc.) */
    cJSON *templates = cJSON_GetObjectItemCaseSensitive(spec->meta, "template_definitions");

    /* §ENFORCE: reject raw C templates — all code must be AST-generated */
    if (templates && cJSON_IsObject(templates) && templates->child) {
        die("spec uses template_definitions — raw C passthrough forbidden. All code must be generated from function_definitions (AST instructions). Convert templates to AST or remove them.");
    }

    /* Phase 2a: compile AST functions to C */
    compile_functions_to_c(spec, out_fp, is_library);

    /* Auto-generate main() for tool mode (not library) */
    if (!is_library) {
        cJSON *func_defs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
        const char *entry = "main";
        if (func_defs && func_defs->child)
            entry = func_defs->child->string;

        fprintf(out_fp, "\n/* Auto-generated entry point */\n");
        fprintf(out_fp, "int main(int argc, char **argv) {\n");
        fprintf(out_fp, "    g_argc = argc;\n");
        fprintf(out_fp, "    g_argv = argv;\n");
        fprintf(out_fp, "    if (argc < 3) {\n");
        fprintf(out_fp, "        fprintf(stderr, \"Usage: %%s <input.ipm> <output.c>\\n\", argv[0]);\n");
        fprintf(out_fp, "        return 1;\n");
        fprintf(out_fp, "    }\n");
        fprintf(out_fp, "    return %s();\n", entry);
        fprintf(out_fp, "}\n");
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
    /* §ENFORCE: file limit per directory — hard fail if >3 .c/.h */
    #ifdef SPEC2C_SRC_DIR
    {
        DIR *d = opendir(SPEC2C_SRC_DIR);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') continue;
                char sub[4096];
                snprintf(sub, sizeof(sub), "%s/%s", SPEC2C_SRC_DIR, de->d_name);
                struct stat st;
                if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
                int cnt = 0;
                DIR *sd = opendir(sub);
                if (sd) {
                    struct dirent *se;
                    while ((se = readdir(sd)) != NULL) {
                        size_t nl = strlen(se->d_name);
                        if (nl > 2 && (!strcmp(se->d_name + nl - 2, ".c") || !strcmp(se->d_name + nl - 2, ".h")))
                            cnt++;
                    }
                    closedir(sd);
                }
                if (cnt > 3) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "SOUL §7: %s has %d .c/.h files (max 3). Split into subdirectories.", sub, cnt);
                    die(buf);
                }
            }
            closedir(d);
        }
    }
    #endif

    const char *spec_path = NULL;
    const char *out_path = NULL;
    const char *base_dir = NULL;
    int check_mode = 0;
    const char *check_spec = NULL;
    int is_library = 0;

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
                "  --help, -h\n"
                "  --library      Generate .c + .h library (skip main), for linking into other programs\n");
            return 0;
        } else if (strcmp(argv[i], "--library") == 0) {
            is_library = 1;
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
        char pat[4096];
        snprintf(pat, sizeof(pat), "%s/soul-patterns.json", base_dir);
        char *args[16]; int ai = 0;
        args[ai++] = (char*)"spec2c-check";
        args[ai++] = (char*)spec_path;
        args[ai++] = (char*)"--base";
        args[ai++] = (char*)base_dir;
        args[ai++] = (char*)"--patterns";
        args[ai++] = pat;
        if (check_spec) {
            args[ai++] = (char*)"--spec";
            args[ai++] = (char*)check_spec;
        }
        args[ai] = NULL;
        return safe_exec(args);
    }

    if (!spec_path) die("spec file required");
    if (!base_dir) base_dir = ".";

    /* Read and parse the spec first to detect .ipm format */
    char *spec_text = read_file(spec_path);
    cJSON *spec_json = cJSON_Parse(spec_text);
    if (!spec_json) die("JSON parse error in spec file");

    /* Structural enforcement — check .ipm file against SOUL §7 limits */
    if (spec_text) {
        int file_lines = 0;
        for (const char *p = spec_text; *p; p++) if (*p == '\n') file_lines++;
        cJSON *limits = cJSON_GetObjectItemCaseSensitive(spec_json, "structural_limits");
        int max_file_lines = 2000, max_funcs = 15, max_func_lines = 250;
        if (limits && cJSON_IsArray(limits)) {
            for (int li = 0; li < cJSON_GetArraySize(limits); li++) {
                cJSON *l = cJSON_GetArrayItem(limits, li);
                const char *ln = jstr(l, "limit_name");
                int mv = cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(l, "maximum_value"))
                    ? cJSON_GetObjectItemCaseSensitive(l, "maximum_value")->valueint : 0;
                if (!strcmp(ln, "file_line_count") && mv) max_file_lines = mv;
                if (!strcmp(ln, "function_count_per_file") && mv) max_funcs = mv;
                if (!strcmp(ln, "function_line_count") && mv) max_func_lines = mv;
            }
        }
        if (file_lines > max_file_lines) {
            char buf[256];
            snprintf(buf, sizeof(buf), "file too long: %d lines (max %d)", file_lines, max_file_lines);
            die(buf);
        }
        cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
        if (funcs && cJSON_IsObject(funcs)) {
            int nfuncs = cJSON_GetArraySize(funcs);
            if (nfuncs > max_funcs) {
                char buf[256];
                snprintf(buf, sizeof(buf), "too many functions: %d (max %d)", nfuncs, max_funcs);
                die(buf);
            }
            cJSON *fn = funcs->child;
            while (fn) {
                int instrs = 0;
                cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
                if (body && cJSON_IsArray(body)) instrs = cJSON_GetArraySize(body);
                if (instrs > max_func_lines) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "function '%s' too long: %d top-level instructions (max %d)",
                             fn->string, instrs, max_func_lines);
                    die(buf);
                }
                fn = fn->next;
            }
        }
    }

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

        generate_from_ipm(&ipm, out_path, is_library);

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
