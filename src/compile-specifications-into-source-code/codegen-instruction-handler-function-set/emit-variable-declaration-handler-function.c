// SPDX-License-Identifier: Apache-2.0
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

typedef void (*instr_hdlr)(cJSON *inst, FILE *out, int indent, const char *return_type);

void emit_function_invocation_code_block(cJSON *inst, FILE *out, int indent) {
    (void)indent;
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

void emit_conditional_branch_code_block(cJSON *inst, FILE *out, int indent, const char *return_type) {
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

void emit_return_statement_code_block(cJSON *inst, FILE *out, const char *return_type) {
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

void emit_iteration_instruction_code_block(cJSON *inst, FILE *out, int indent, const char *return_type) {
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

void emit_variable_declaration_into_output(cJSON *inst, FILE *out, int indent, const char *return_type) {
    (void)return_type;
    const char *vn = extract_json_field_string_value(inst, "variable_name");
    const char *vt = extract_json_field_string_value(inst, "variable_type");
    const char *op = extract_json_field_string_value(inst, "assignment_operation");
    cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
    const char *src = "";
    if (cJSON_IsString(st)) src = st->valuestring;
    else if (st) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(st, "source"); if (cJSON_IsString(s2)) src = s2->valuestring; }
    if (!vn || !vt || !op) return;
    cJSON *arr_sz = cJSON_GetObjectItemCaseSensitive(inst, "array_size");
    cJSON *fields  = cJSON_GetObjectItemCaseSensitive(inst, "struct_fields");
    int array_size = (arr_sz && cJSON_IsNumber(arr_sz)) ? arr_sz->valueint : 0;
    if (array_size > 1 && fields && cJSON_IsArray(fields)) {
        cJSON *field;
        cJSON_ArrayForEach(field, fields) {
            if (!cJSON_IsString(field)) continue;
            char fn[64] = {0}, ft[64] = {0};
            if (sscanf(field->valuestring, "%63[^:]:%63s", fn, ft) == 2) {
                for (int s = 0; s < indent; s++) fputs("  ", out);
                fprintf(out, "%s %s_%s[%d];\n", resolve_spec_type_into_lang(ft), vn, fn, array_size);
            }
        }
        for (int s = 0; s < indent; s++) fputs("  ", out);
        fprintf(out, "%s(%s", op, src ? src : "");
        cJSON_ArrayForEach(field, fields) {
            if (!cJSON_IsString(field)) continue;
            char fn[64] = {0}, ft[64] = {0};
            if (sscanf(field->valuestring, "%63[^:]:%63s", fn, ft) == 2)
                fprintf(out, ", %s_%s", vn, fn);
        }
        fprintf(out, ", %d);\n", array_size);
    } else {
        fprintf(out, "%s %s = %s(%s);\n", resolve_spec_type_into_lang(vt), vn, op, src);
    }
}

static void emit_new_standard_loop_code(cJSON *inst, FILE *out, int indent, const char *return_type) {
    const char *ty = extract_json_field_string_value(inst, "instruction_type");
    if (!strcmp(ty, "string_tokenizer_loop")) {
        const char *src = extract_json_field_string_value(inst, "source_string");
        const char *sep = extract_json_field_string_value(inst, "separator");
        const char *tok = extract_json_field_string_value(inst, "token_variable");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
        if (!src[0] || !sep[0] || !tok[0] || !body) return;
        fprintf(out, "{\n");
        fprintf(out, "  char _buf[256]; snprintf(_buf, sizeof(_buf), \"%%s\", %s);\n", src);
        fprintf(out, "  char *_save;\n");
        fprintf(out, "  for (char *_t = strtok_r(_buf, \"%s\", &_save); _t; _t = strtok_r(NULL, \"%s\", &_save)) {\n", sep, sep);
        fprintf(out, "    const char *%s = _t;\n", tok);
        generate_code_from_ast_instructions(body, out, indent + 1, return_type);
        fprintf(out, "  }\n");
        fprintf(out, "}\n");
    } else if (!strcmp(ty, "for_count_loop")) {
        const char *cv = extract_json_field_string_value(inst, "counter_variable");
        const char *lv = extract_json_field_string_value(inst, "limit_variable");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
        if (!cv[0] || !lv[0] || !body) return;
        fprintf(out, "for (int %s = 0; %s < %s; %s++) {\n", cv, cv, lv, cv);
        generate_code_from_ast_instructions(body, out, indent + 1, return_type);
        fprintf(out, "}\n");
    } else if (!strcmp(ty, "emit_formatted_code")) {
        const char *fmt = extract_json_field_string_value(inst, "code_format");
        cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "code_arguments");
        if (!fmt[0]) return;
        fprintf(out, "fprintf(out, \"");
        for (const char *p = fmt; *p; p++) {
            if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
            else if (*p == '\n') fprintf(out, "\\n");
            else fputc(*p, out);
        }
        fprintf(out, "\"");
        if (args && cJSON_IsArray(args)) {
            for (int a = 0; a < cJSON_GetArraySize(args); a++) {
                cJSON *arg = cJSON_GetArrayItem(args, a);
                if (cJSON_IsString(arg) && arg->valuestring[0])
                    fprintf(out, ", %s", arg->valuestring);
            }
        }
        fprintf(out, ");\n");
    } else if (!strcmp(ty, "generate_body_code")) {
        const char *bf = extract_json_field_string_value(inst, "body_field");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, bf);
        if (body) generate_code_from_ast_instructions(body, out, indent + 1, return_type);
    }
}

static void emit_invocation_code_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)rt; emit_function_invocation_code_block(inst, out, indent);
}
static void emit_branch_code_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    emit_conditional_branch_code_block(inst, out, indent, rt);
}
static void emit_return_code_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)indent; emit_return_statement_code_block(inst, out, rt);
}
static void emit_iterate_code_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    emit_iteration_instruction_code_block(inst, out, indent, rt);
}
static void emit_field_access_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)indent; (void)rt;
    const char *vn = extract_json_field_string_value(inst, "variable_name");
    const char *vt = extract_json_field_string_value(inst, "variable_type");
    const char *so = extract_json_field_string_value(inst, "source_object");
    const char *fn = extract_json_field_string_value(inst, "field_name");
    if (!vn[0] || !so[0] || !fn[0]) return;
    if (vt && (!strcmp(vt, "string") || !strcmp(vt, "char")))
        fprintf(out, "const char *%s = cJSON_GetObjectItemCaseSensitive(%s,\"%s\") ? cJSON_GetObjectItemCaseSensitive(%s,\"%s\")->valuestring : \"\";\n", vn, so, fn, so, fn);
    else fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
}

typedef struct {
    const char *type;
    instr_hdlr handler;
} instr_dispatch_t;

static const instr_dispatch_t INSTR_HANDLERS[] = {
    {"access_json_field",              emit_field_access_into_output},
    {"conditional_branch",             emit_branch_code_into_output},
    {"database_execute_parameterized", emit_new_standard_loop_code},
    {"emit_formatted_code",            emit_new_standard_loop_code},
    {"for_count_loop",                 emit_new_standard_loop_code},
    {"function_invocation",            emit_invocation_code_into_output},
    {"generate_body_code",             emit_new_standard_loop_code},
    {"iterate_over_collection",        emit_iterate_code_into_output},
    {"iterate_over_object_keys",       emit_iterate_code_into_output},
    {"return_statement",               emit_return_code_into_output},
    {"string_tokenizer_loop",          emit_new_standard_loop_code},
    {"variable_declaration",           emit_variable_declaration_into_output},
    {NULL, NULL}
};

void generate_code_via_dispatch_table(cJSON *instructions, FILE *out, int indent, const char *return_type) {
    if (!cJSON_IsArray(instructions)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(instructions); ii++) {
        cJSON *inst = cJSON_GetArrayItem(instructions, ii);
        if (!inst) continue;
        cJSON *it = cJSON_GetObjectItemCaseSensitive(inst, "instruction_type");
        if (!cJSON_IsString(it)) continue;
        for (int s = 0; s < indent; s++) fputs("  ", out);
        for (int d = 0; INSTR_HANDLERS[d].type; d++)
            if (!strcmp(it->valuestring, INSTR_HANDLERS[d].type))
                { INSTR_HANDLERS[d].handler(inst, out, indent, return_type); break; }
    }
}
