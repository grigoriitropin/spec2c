// SPDX-License-Identifier: Apache-2.0
// spec2c codegen — C bootstrap: 3 primitives + dispatcher
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

typedef void (*instr_hdlr)(cJSON *inst, FILE *out, int indent, const char *return_type);

static void emit_formatted_code_primitive_handler(cJSON *inst, FILE *out) {
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
}

static void emit_conditional_branch_code_primitive(cJSON *inst, FILE *out, int indent, const char *return_type) {
    const char *op = extract_json_field_string_value(inst, "condition_operation");
    const char *tgt = extract_json_field_string_value(inst, "condition_target");
    const char *cv = extract_json_field_string_value(inst, "condition_value");
    if (!strcmp(op, "key_exists"))
        fprintf(out, "if (cJSON_HasObjectItem(%s, \"%s\")) {\n", tgt, cv ? cv : "");
    else if (!strcmp(op, "is_not_null"))
        fprintf(out, "if (%s != NULL) {\n", tgt);
    else
        fprintf(out, "if (strcmp(%s, \"%s\") == 0) {\n", tgt, cv ? cv : "");
    cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
    if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
    fprintf(out, "} else {\n");
    cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
    if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
    fprintf(out, "}\n");
}

static void emit_variable_declaration_code_line(cJSON *inst, FILE *out, int indent) {
    const char *vn = extract_json_field_string_value(inst, "variable_name");
    const char *vt = extract_json_field_string_value(inst, "variable_type");
    const char *op = extract_json_field_string_value(inst, "assignment_operation");
    cJSON *arr_sz = cJSON_GetObjectItemCaseSensitive(inst, "array_size");
    cJSON *fields  = cJSON_GetObjectItemCaseSensitive(inst, "struct_fields");
    int array_size = (arr_sz && cJSON_IsNumber(arr_sz)) ? arr_sz->valueint : 0;
    if (array_size > 1 && fields && cJSON_IsArray(fields)) {
        cJSON *field;
        cJSON_ArrayForEach(field, fields) {
            if (!cJSON_IsString(field)) continue;
            char fn[64] = {0}, ft[64] = {0};
            if (sscanf(field->valuestring, "%63[^:]:%63s", fn, ft) == 2) {
                fprintf(out, "%*c%s %s_%s[%d];\n",
                    indent * 2, ' ', resolve_spec_type_into_lang(ft), vn, fn, array_size);
            }
        }
        return;
    }
    (void)vt;
    if (!strcmp(op, "literal")) {
        cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
        if (cJSON_IsNumber(st))
            fprintf(out, "  %s %s = %d;\n", resolve_spec_type_into_lang(vt), vn, st->valueint);
        else if (cJSON_IsString(st))
            fprintf(out, "  const char *%s = \"%s\";\n", vn, st->valuestring);
        else
            fprintf(out, "  %s %s = 0;\n", resolve_spec_type_into_lang(vt), vn);
        return;
    }
    fprintf(out, "%s %s = %s(%s);\n", vt, vn, op,
        extract_json_field_string_value(inst, "source_target"));
}


static int emit_builtin_call_when_matched(cJSON *inst, FILE *out) {
    const char *fn = extract_json_field_string_value(inst, "invocation_name");
    const char *rv = extract_json_field_string_value(inst, "result_assignment_variable");
    cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
    if (!strcmp(fn, "report_error_and_exit"))
        return emit_report_error_then_exit(inst, out);
    if (!strcmp(fn, "system_exit")) {
        int code = 0;
        if (args && cJSON_IsArray(args) && cJSON_GetArraySize(args) > 0) {
            cJSON *a0 = cJSON_GetArrayItem(args, 0);
            if (cJSON_IsNumber(a0)) code = a0->valueint;
            else if (cJSON_IsObject(a0))
                code = atoi(extract_json_field_string_value(a0, "value"));
            else if (cJSON_IsString(a0))
                code = atoi(a0->valuestring);
        }
        fprintf(out, "  exit(%d);\n", code);
        return 1;
    }
    if (!strcmp(fn, "get_cli_arg")) {
        int idx = 0;
        if (args && cJSON_IsArray(args) && cJSON_GetArraySize(args) > 0) {
            cJSON *a0 = cJSON_GetArrayItem(args, 0);
            if (cJSON_IsNumber(a0)) idx = a0->valueint;
            else if (cJSON_IsObject(a0))
                idx = atoi(extract_json_field_string_value(a0, "value"));
            else if (cJSON_IsString(a0))
                idx = atoi(a0->valuestring);
        }
        if (rv[0]) fprintf(out, "  const char *%s = ", rv);
        fprintf(out, "(%d < argc ? argv[%d] : NULL);\n", idx, idx);
        return 1;
    }
    return 0;
}

static void emit_function_invocation_with_arguments(cJSON *inst, FILE *out, int indent) {
    (void)indent;
    const char *fn = extract_json_field_string_value(inst, "invocation_name");
    const char *rv = extract_json_field_string_value(inst, "result_assignment_variable");
    cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
    if (!fn[0]) return;
    if (emit_builtin_call_when_matched(inst, out)) return;
    if (rv[0]) fprintf(out, "  char *%s = ", rv);
    fprintf(out, "%s(", fn);
    if (args) {
        if (cJSON_IsObject(args)) {
            cJSON *arg = args->child;
            int first = 1;
            while (arg) {
                if (!first) fprintf(out, ", ");
                if (cJSON_IsString(arg))
                    fprintf(out, "%s", arg->valuestring);
                else if (cJSON_IsNumber(arg))
                    fprintf(out, "%d", arg->valueint);
                first = 0;
                arg = arg->next;
            }
        } else if (cJSON_IsArray(args)) {
            for (int ai = 0; ai < cJSON_GetArraySize(args); ai++) {
                cJSON *arg = cJSON_GetArrayItem(args, ai);
                if (!arg) continue;
                if (ai > 0) fprintf(out, ", ");
                if (cJSON_IsString(arg))
                    fprintf(out, "%s", arg->valuestring);
                else if (cJSON_IsObject(arg)) {
                    const char *kind = extract_json_field_string_value(arg, "kind");
                    const char *val = extract_json_field_string_value(arg, "value");
                    if (!strcmp(kind, "str"))
                        fprintf(out, "\"%s\"", val);
                    else
                        fprintf(out, "%s", val);
                }
            }
        }
    }
    fprintf(out, ");\n");
}

static void emit_scan_directory_with_body(cJSON *inst, FILE *out, int indent) {
    const char *dp = extract_json_field_string_value(inst, "directory_path");
    if (!dp[0]) return;
    fprintf(out, "  cJSON *_entries = list_files_inside_directory_path(%s);\n", dp);
    fprintf(out, "  if (_entries) {\n");
    fprintf(out, "    for (int _i = 0; _i < cJSON_GetArraySize(_entries); _i++) {\n");
    fprintf(out, "      cJSON *_entry = cJSON_GetArrayItem(_entries, _i);\n");
    fprintf(out, "      const char *_name = cJSON_GetObjectItemCaseSensitive(_entry, \"name\")->valuestring;\n");
    /* filter: only .c and .h files */
    fprintf(out, "      size_t _nl = strlen(_name);\n");
    fprintf(out, "      if (_nl < 3) continue;\n");
    fprintf(out, "      if (strcmp(_name + _nl - 2, \".c\") && strcmp(_name + _nl - 2, \".h\") &&\n");
    fprintf(out, "          strcmp(_name + _nl - 4, \".ipm\")) continue;\n");
    /* compile-time body processing */
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
    if (body) generate_code_from_ast_instructions(body, out, indent + 3, "i32");
    fprintf(out, "    }\n");
    fprintf(out, "    cJSON_Delete(_entries);\n");
    fprintf(out, "  }\n");
}

static void emit_alu_operation_into_output(cJSON *inst, FILE *out) {
    const char *op = extract_json_field_string_value(inst, "operator");
    const char *tgt = extract_json_field_string_value(inst, "target");
    const char *tt = extract_json_field_string_value(inst, "target_type");
    const char *tl = resolve_spec_type_into_lang(tt);
    cJSON *lhs = cJSON_GetObjectItemCaseSensitive(inst, "lhs");
    cJSON *rhs = cJSON_GetObjectItemCaseSensitive(inst, "rhs");
    const char *lv = "", *rv = "";
    if (lhs && cJSON_IsObject(lhs)) lv = extract_json_field_string_value(lhs, "value");
    if (rhs && cJSON_IsObject(rhs)) rv = extract_json_field_string_value(rhs, "value");
    if (!lv[0] || !tgt[0]) return;

    if (!strcmp(op, "~")) {
        fprintf(out, "  %s %s = ~(%s);\n", tl, tgt, lv);
    } else if (!strcmp(op, "ROTR")) {
        fprintf(out, "  %s %s = ((%s) >> (%s)) | ((%s) << (32 - (%s)));\n",
            tl, tgt, lv, rv, lv, rv);
    } else {
        /* division by zero guard */
        if (!strcmp(op, "/") || !strcmp(op, "%"))
            fprintf(out, "  if ((%s) == 0) exit(255);\n", rv);
        fprintf(out, "  %s %s = (%s) %s (%s);\n", tl, tgt, lv, op, rv);
    }
}

static void emit_iteration_loop_with_count(cJSON *inst, FILE *out, int indent, const char *rt) {
    const char *cv = extract_json_field_string_value(inst, "counter_variable");
    const char *lv = extract_json_field_string_value(inst, "limit_variable");
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
    if (!cv[0] || !lv[0]) return;
    fprintf(out, "  for (int %s = 0; %s < %s; %s++) {\n", cv, cv, lv, cv);
    if (body) generate_code_from_ast_instructions(body, out, indent + 2, rt);
    fprintf(out, "  }\n");
}


static void emit_string_tokenizer_with_body(cJSON *inst, FILE *out, int indent, const char *rt) {
    const char *src = extract_json_field_string_value(inst, "source_string");
    const char *sep = extract_json_field_string_value(inst, "separator");
    const char *tok = extract_json_field_string_value(inst, "token_variable");
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
    if (!src[0] || !sep[0]) return;
    fprintf(out, "  {\n");
    fprintf(out, "    char _buf[256]; snprintf(_buf, sizeof(_buf), \"%%s\", %s);\n", src);
    fprintf(out, "    char *_save;\n");
    fprintf(out, "    for (char *_t = strtok_r(_buf, \"%s\", &_save); _t; _t = strtok_r(NULL, \"%s\", &_save)) {\n", sep, sep);
    if (tok[0]) fprintf(out, "      const char *%s = _t;\n", tok);
    if (body) generate_code_from_ast_instructions(body, out, indent + 3, rt);
    fprintf(out, "    }\n");
    fprintf(out, "  }\n");
    return;
}

static void emit_bootstrap_central_dispatcher_func(cJSON *inst, FILE *out, int indent, const char *return_type) {
    (void)indent;
    const char *ty = extract_json_field_string_value(inst, "instruction_type");
    if (!strcmp(ty, "alu_operation")) { emit_alu_operation_into_output(inst, out); return; }
    if (!strcmp(ty, "conditional_branch")) { emit_conditional_branch_code_primitive(inst, out, indent, return_type); return; }
    if (!strcmp(ty, "emit_formatted_code")) { emit_formatted_code_primitive_handler(inst, out); return; }
    if (!strcmp(ty, "for_count_loop")) { emit_iteration_loop_with_count(inst, out, indent, return_type); return; }
    if (!strcmp(ty, "function_invocation")) { emit_function_invocation_with_arguments(inst, out, indent); return; }
    if (!strcmp(ty, "read_file_content")) {
        const char *fp = extract_json_field_string_value(inst, "file_path");
        const char *rv = extract_json_field_string_value(inst, "result_variable");
        if (fp[0] && rv[0])
            fprintf(out, "  char *%s = read_entire_file_into_string(%s);\n", rv, fp);
        return;
    }
    if (!strcmp(ty, "scan_directory_entries")) { emit_scan_directory_with_body(inst, out, indent); return; }
    if (!strcmp(ty, "string_tokenizer_loop")) { emit_string_tokenizer_with_body(inst, out, indent, return_type); return; }
    if (!strcmp(ty, "variable_declaration")) { emit_variable_declaration_code_line(inst, out, indent); return; }
    if (!strcmp(ty, "alu_operation")) { emit_alu_operation_into_output(inst, out); return; }
    if (!strcmp(ty, "scan_directory_entries")) { emit_scan_directory_with_body(inst, out, indent); return; }
}

typedef struct {
    const char *type;
    instr_hdlr handler;
} dispatch_entry_t;
static const dispatch_entry_t INSTR_HANDLERS[] = {
    {"access_json_field",         emit_bootstrap_central_dispatcher_func},
    {"alu_operation",             emit_bootstrap_central_dispatcher_func},
    {"conditional_branch",        emit_bootstrap_central_dispatcher_func},
    {"emit_formatted_code",       emit_bootstrap_central_dispatcher_func},
    {"for_count_loop",            emit_bootstrap_central_dispatcher_func},
    {"function_invocation",       emit_bootstrap_central_dispatcher_func},
    {"iterate_over_collection",   emit_bootstrap_central_dispatcher_func},
    {"iterate_over_object_keys",  emit_bootstrap_central_dispatcher_func},
    {"read_file_content",         emit_bootstrap_central_dispatcher_func},
    {"return_statement",          emit_bootstrap_central_dispatcher_func},
    {"scan_directory_entries",    emit_bootstrap_central_dispatcher_func},
    {"string_tokenizer_loop",     emit_bootstrap_central_dispatcher_func},
    {"variable_declaration",      emit_bootstrap_central_dispatcher_func},
    {NULL, NULL}
};

void generate_code_via_dispatch_table(cJSON *insts, FILE *out, int indent, const char *rt) {
    if (!cJSON_IsArray(insts)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(insts); ii++) {
        cJSON *inst = cJSON_GetArrayItem(insts, ii);
        if (!inst) continue;
        cJSON *it = cJSON_GetObjectItemCaseSensitive(inst, "instruction_type");
        if (!cJSON_IsString(it)) continue;
        for (int s = 0; s < indent; s++) fputs("  ", out);
        for (int d = 0; INSTR_HANDLERS[d].type; d++)
            if (!strcmp(it->valuestring, INSTR_HANDLERS[d].type))
                { INSTR_HANDLERS[d].handler(inst, out, indent, rt); break; }
    }
}

