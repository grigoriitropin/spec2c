// SPDX-License-Identifier: Apache-2.0
// gen.c — AST-to-C code generation

#include "shared-type-declarations-across-modules/share-type-definitions-across-files.h"

void append_key_value_into_substitution(subst_t *subs, int *n, const char *k, const char *v) {
    if (*n >= SUBST_MAX) report_fatal_error_and_exit("too many substitutions");
    subs[*n].key = k;
    snprintf(subs[*n].val, VAL_SZ, "%s", v ? v : "");
    (*n)++;
}

static void emit_function_invocation_code_block(cJSON *inst, FILE *out, int indent) {
    const char *fn = extract_json_field_string_value(inst, "invocation_name");
    const char *rv = extract_json_field_string_value(inst, "result_assignment_variable");
    const char *rt = extract_json_field_string_value(inst, "result_type");
    cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
    if (!fn[0]) return;
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
            if (strstr(fn, "read_entire_file_into_memory")) rc = "char *";
            else if (strstr(fn, "parse_json")) rc = "cJSON *";
            else if (strstr(fn, "allocate_and_init_hash_table")) rc = "subst_table *";
            else if (strstr(fn, "hash_table_lookup_key_value")) rc = "const char *";
            else if (!strcmp(fn, "apply_substitution_against_raw_text")) rc = "char *";
            else if (!strcmp(fn, "create_substitution_table_for_spec")) rc = "subst_table *";
        }
        fprintf(out, "%s %s = ", rc, rv);
    }
    fprintf(out, "%s(", fn);
    if (args && cJSON_IsObject(args)) {
        cJSON *arg = args->child; int first = 1;
        while (arg) {
            if (!first) fprintf(out, ", ");
            if (cJSON_IsString(arg)) fprintf(out, "%s", arg->valuestring);
            else if (cJSON_IsNumber(arg)) fprintf(out, "\"%d\"", arg->valueint);
            first = 0; arg = arg->next;
        }
    }
    fprintf(out, ");\n");
}

static void emit_conditional_branch_code_block(cJSON *inst, FILE *out, int indent, const char *return_type) {
    const char *op = extract_json_field_string_value(inst, "condition_operation");
    cJSON *ct = cJSON_GetObjectItemCaseSensitive(inst, "condition_target");
    const char *cv = extract_json_field_string_value(inst, "condition_value");
    const char *ck = extract_json_field_string_value(inst, "condition_key");
    const char *tgt = "";
    if (cJSON_IsString(ct)) tgt = ct->valuestring;
    else if (ct) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(ct, "source"); if (cJSON_IsString(s2)) tgt = s2->valuestring; }
    if (!strcmp(op, "key_exists"))
        fprintf(out, "if (cJSON_HasObjectItem(%s, \"%s\")) {\n", tgt, ck ? ck : "");
    else if (!strcmp(op, "string_equals") || !strcmp(op, "enum_equals"))
        fprintf(out, "if (strcmp(%s, \"%s\") == 0) {\n", tgt, cv ? cv : "");
    else if (!strcmp(op, "is_not_null"))
        fprintf(out, "if (%s != NULL) {\n", tgt);
    else
        fprintf(out, "if (/* %s */ 0) {\n", op);
    cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
    generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
    fprintf(out, "%*c} else {\n", indent * 2, ' ');
    cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
    generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
    fprintf(out, "%*c}\n", indent * 2, ' ');
}

static void emit_return_statement_code_block(cJSON *inst, FILE *out, const char *return_type) {
    cJSON *rp = cJSON_GetObjectItemCaseSensitive(inst, "return_payload");
    int is_void = return_type && !strcmp(return_type, "void");
    if (rp) {
        const char *es = extract_json_field_string_value(rp, "execution_status");
        const char *ec = extract_json_field_string_value(rp, "error_code");
        const char *rv = extract_json_field_string_value(rp, "value");
        if (es && !strcmp(es, "failure"))
            fprintf(out, "report_fatal_error_and_exit(\"%s\");%s\n", ec[0] ? ec : "unknown error", is_void ? "" : " return 1;");
        else if (rv[0])
            fprintf(out, "return %s;\n", rv);
        else if (is_void)
            fprintf(out, "return;\n");
        else
            fprintf(out, "return 0;\n");
    } else {
        fprintf(out, "%s\n", is_void ? "return;" : "return 0;");
    }
}

static void emit_iteration_instruction_code_block(cJSON *inst, FILE *out, int indent, const char *return_type) {
    const char *type = extract_json_field_string_value(inst, "instruction_type");
    if (!strcmp(type, "iterate_over_collection")) {
        const char *col = extract_json_field_string_value(inst, "collection_variable");
        const char *item = extract_json_field_string_value(inst, "item_variable");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
        if (col[0] && item[0] && body) {
            fprintf(out, "for (int _i_%s = 0; _i_%s < cJSON_GetArraySize(%s); _i_%s++) {\n", col, col, col, col);
            fprintf(out, "  cJSON *%s = cJSON_GetArrayItem(%s, _i_%s);\n", item, col, col);
            generate_code_from_ast_instructions(body, out, indent + 1, return_type);
            fprintf(out, "%*c}\n", indent * 2, ' ');
        }
    } else {
        const char *col = extract_json_field_string_value(inst, "collection_variable");
        const char *key  = extract_json_field_string_value(inst, "key_variable");
        const char *val  = extract_json_field_string_value(inst, "value_variable");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
        if (col[0] && val[0] && body) {
            fprintf(out, "cJSON *%s = %s;\n", val, col);
            fprintf(out, "cJSON_ArrayForEach(%s, %s) {\n", val, col);
            if (key) fprintf(out, "  const char *%s = %s->string;\n", key, val);
            generate_code_from_ast_instructions(body, out, indent + 1, return_type);
            fprintf(out, "}\n");
        }
    }
}

void generate_code_from_ast_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type) {
    if (!cJSON_IsArray(instructions)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(instructions); ii++) {
        cJSON *inst = cJSON_GetArrayItem(instructions, ii);
        if (!inst) continue;
        cJSON *it = cJSON_GetObjectItemCaseSensitive(inst, "instruction_type");
        if (!cJSON_IsString(it)) continue;
        const char *type = it->valuestring;
        for (int s = 0; s < indent; s++) fputs("  ", out);

        if (!strcmp(type, "variable_declaration")) {
            const char *vn = extract_json_field_string_value(inst, "variable_name");
            const char *vt = extract_json_field_string_value(inst, "variable_type");
            const char *op = extract_json_field_string_value(inst, "assignment_operation");
            cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
            const char *src = "";
            if (cJSON_IsString(st)) src = st->valuestring;
            else if (st) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(st, "source"); if (cJSON_IsString(s2)) src = s2->valuestring; }
            if (vn && vt && op) {
                fprintf(out, "%s %s = %s(%s);\n", resolve_spec_type_into_lang(vt), vn, op, src);
            }
        } else if (!strcmp(type, "function_invocation")) {
            emit_function_invocation_code_block(inst, out, indent);
        } else if (!strcmp(type, "conditional_branch")) {
            emit_conditional_branch_code_block(inst, out, indent, return_type);
        } else if (!strcmp(type, "return_statement")) {
            emit_return_statement_code_block(inst, out, return_type);
        } else if (!strcmp(type, "iterate_over_collection") || !strcmp(type, "iterate_over_object_keys")) {
            emit_iteration_instruction_code_block(inst, out, indent, return_type);
        } else if (!strcmp(type, "access_json_field")) {
            const char *vn = extract_json_field_string_value(inst, "variable_name");
            const char *vt = extract_json_field_string_value(inst, "variable_type");
            const char *so = extract_json_field_string_value(inst, "source_object");
            const char *fn = extract_json_field_string_value(inst, "field_name");
            if (vn[0] && so[0] && fn[0]) {
                if (vt && (!strcmp(vt, "string") || !strcmp(vt, "char")))
                    fprintf(out, "const char *%s = cJSON_GetObjectItemCaseSensitive(%s,\"%s\") ? cJSON_GetObjectItemCaseSensitive(%s,\"%s\")->valuestring : \"\";\n", vn, so, fn, so, fn);
                else
                    fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
            }
        } else if (!strcmp(type, "database_execute_parameterized")) {
            const char *sql = extract_json_field_string_value(inst, "sql_query_string");
            fprintf(out, "/* DB exec: %s */\n", sql ? sql : "?");
        }
    }
}

static const char *resolve_function_return_type_code(const char *ret) {
    if (!ret[0]) return "void";
    if (!strcmp(ret, "void")) return "void";
    if (!strcmp(ret, "int")) return "int";
    if (!strcmp(ret, "double")) return "double";
    if (!strcmp(ret, "boolean")) return "int";
    if (!strcmp(ret, "string") || !strcmp(ret, "char")) return "char *";
    if (!strcmp(ret, "json_object")) return "cJSON *";
    if (!strcmp(ret, "json_array")) return "cJSON *";
    if (!strcmp(ret, "db_handle")) return "struct vehir_db *";
    if (!strcmp(ret, "subst_table")) return "subst_table *";
    return "void";
}

static void emit_function_body_into_output(cJSON *fn, FILE *out, int is_library, int has_modname) {
    const char *name = fn->string;
    cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
    cJSON *body  = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
    if (!body || !cJSON_IsArray(body)) return;
    const char *ret = extract_json_field_string_value(fn, "return_type");
    fprintf(out, "%s%s %s(", is_library ? "" : (has_modname ? "" : "static "),
        resolve_function_return_type_code(ret), name);
    if (params && cJSON_IsArray(params)) {
        for (int p = 0; p < cJSON_GetArraySize(params); p++) {
            cJSON *par = cJSON_GetArrayItem(params, p);
            const char *pn = extract_json_field_string_value(par, "parameter_name");
            const char *pt = extract_json_field_string_value(par, "parameter_type");
            if (p > 0) fprintf(out, ", ");
            fprintf(out, "%s %s", resolve_spec_type_into_lang(pt), pn);
        }
    }
    fprintf(out, ") {\n");
    generate_code_from_ast_instructions(body, out, 1, ret);
    fprintf(out, "}\n\n");
}

void compile_every_function_into_code(const ipm_spec_t *spec, FILE *out, int is_library) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn;
    const char *modname = extract_json_field_string_value(spec->meta, "module_name");
    int has_mod = modname[0] != 0;
    if (has_mod) {
        fprintf(out, "#include \"%s.h\"\n\n", modname);
    } else {
        fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n#include \"ipm_builtins.h\"\n\n");
        fn = funcs->child;
        while (fn) {
            const char *name = fn->string;
            fprintf(out, "%s%s %s(", is_library ? "" : "static ",
                resolve_function_return_type_code(extract_json_field_string_value(fn, "return_type")), name);
            cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    if (p > 0) fprintf(out, ", ");
                    fprintf(out, "%s %s", resolve_spec_type_into_lang(
                        extract_json_field_string_value(par, "parameter_type")),
                        extract_json_field_string_value(par, "parameter_name"));
                }
            }
            fprintf(out, ");\n");
            fn = fn->next;
        }
        fprintf(out, "\n");
    }
    fn = funcs->child;
    while (fn) {
        emit_function_body_into_output(fn, out, is_library, has_mod);
        fn = fn->next;
    }
}
