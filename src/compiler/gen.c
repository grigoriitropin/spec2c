// SPDX-License-Identifier: Apache-2.0
// gen.c — AST-to-C code generation

#include "common_h/common.h"

void ipm_add_subst(subst_t *subs, int *n, const char *k, const char *v) {
    if (*n >= SUBST_MAX) die("too many substitutions");
    subs[*n].key = k;
    snprintf(subs[*n].val, VAL_SZ, "%s", v ? v : "");
    (*n)++;
}

void compile_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type) {
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
            const char *rt = jstr(inst, "result_type");
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
                if (args && cJSON_IsObject(args)) {
                    cJSON *arg = args->child; int first = 1;
                    while (arg) {
                        if (!first) fprintf(out, ", ");
                        if (cJSON_IsString(arg)) {
                            const char *sv = arg->valuestring;
                            if (sv[0] == '"') fprintf(out, "%s", sv);
                            else fprintf(out, "%s", sv);
                        } else if (cJSON_IsNumber(arg)) fprintf(out, "\"%d\"", arg->valueint);
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

void compile_functions_to_c(const ipm_spec_t *spec, FILE *out, int is_library) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn;
    const char *modname = jstr(spec->meta, "module_name");
    if (modname[0]) {
        fprintf(out, "#include \"%s.h\"\n\n", modname);
    } else {
        fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n#include \"ipm_builtins.h\"\n\n");
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
