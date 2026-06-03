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

static void emit_bootstrap_central_dispatcher_func(cJSON *inst, FILE *out, int indent, const char *return_type) {
    (void)indent;
    const char *ty = extract_json_field_string_value(inst, "instruction_type");
    if (!strcmp(ty, "emit_formatted_code")) { emit_formatted_code_primitive_handler(inst, out); return; }
    if (!strcmp(ty, "conditional_branch")) { emit_conditional_branch_code_primitive(inst, out, indent, return_type); return; }
    if (!strcmp(ty, "variable_declaration")) {
        const char *vn = extract_json_field_string_value(inst, "variable_name");
        const char *vt = extract_json_field_string_value(inst, "variable_type");
        const char *op = extract_json_field_string_value(inst, "assignment_operation");
        const char *src = extract_json_field_string_value(inst, "source_target");
        (void)vn;
        (void)vt;
        (void)op;
        (void)src;
        fprintf(out, "%s %s = %s(%s);\n", vt, vn, op, src);
        return;
    }
}

typedef struct { const char *type; instr_hdlr handler; } dispatch_entry_t;
static const dispatch_entry_t INSTR_HANDLERS[] = {
    {"access_json_field",         emit_bootstrap_central_dispatcher_func},
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
