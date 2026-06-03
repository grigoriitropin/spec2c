// SPDX-License-Identifier: Apache-2.0
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

typedef void (*instr_hdlr)(cJSON *inst, FILE *out, int indent, const char *return_type);

static void emit_variable_declaration_into_output(cJSON *inst, FILE *out, int indent, const char *return_type) {
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

static void emit_invocation_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)rt; emit_function_invocation_code_block(inst, out, indent);
}
static void emit_branch_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    emit_conditional_branch_code_block(inst, out, indent, rt);
}
static void emit_return_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)indent; emit_return_statement_code_block(inst, out, rt);
}
static void emit_iterate_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    emit_iteration_instruction_code_block(inst, out, indent, rt);
}
static void emit_field_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
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
static void emit_dbexec_into_output(cJSON *inst, FILE *out, int indent, const char *rt) {
    (void)indent; (void)rt;
    const char *sql = extract_json_field_string_value(inst, "sql_query_string");
    fprintf(out, "/* DB exec: %s */\n", sql ? sql : "?");
}

typedef struct { const char *type; instr_hdlr handler; } instr_dispatch_t;

static const instr_dispatch_t INSTR_HANDLERS[] = {
    {"access_json_field",              emit_field_into_output},
    {"conditional_branch",             emit_branch_into_output},
    {"database_execute_parameterized", emit_dbexec_into_output},
    {"function_invocation",            emit_invocation_into_output},
    {"iterate_over_collection",        emit_iterate_into_output},
    {"iterate_over_object_keys",       emit_iterate_into_output},
    {"return_statement",               emit_return_into_output},
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
