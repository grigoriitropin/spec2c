// SPDX-License-Identifier: Apache-2.0
// IPM-powered handler dispatch — C bootstrap: emit_formatted_code only
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

typedef void (*instr_hdlr)(cJSON *inst, FILE *out, int indent, const char *return_type);

/* ── IPM-compiled handlers (generated from spec-handler-definition-compendium-one.ipm) ── */
extern void emit_json_field_access_handler(cJSON *, FILE *, int, const char *);
extern void emit_return_statement_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_function_invocation_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_iterate_collection_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_object_keys_iterate_handler(cJSON *, FILE *, int, const char *);
extern void emit_tokenizer_loop_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_count_loop_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_branch_condition_handler_code(cJSON *, FILE *, int, const char *);
extern void emit_variable_decl_handler_code(cJSON *, FILE *, int, const char *);

/* ── C bootstrap: emit_formatted_code ───────────────────────────────── */
static void emit_code_format_primitive_handler(cJSON *inst, FILE *out, int indent, const char *return_type) {
    (void)indent; (void)return_type;
    const char *ty = extract_json_field_string_value(inst, "instruction_type");
    if (!strcmp(ty, "emit_formatted_code")) {
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
}

/* ── Dispatch table: C bootstrap + IPM handlers ─────────────────────── */
typedef struct { const char *type; instr_hdlr handler; } instr_dispatch_t;

static const instr_dispatch_t INSTR_HANDLERS[] = {
    {"access_json_field",              emit_json_field_access_handler},
    {"conditional_branch",             emit_branch_condition_handler_code},
    {"emit_formatted_code",            emit_code_format_primitive_handler},
    {"for_count_loop",                 emit_count_loop_handler_code},
    {"function_invocation",            emit_function_invocation_handler_code},
    {"iterate_over_collection",        emit_iterate_collection_handler_code},
    {"iterate_over_object_keys",       emit_object_keys_iterate_handler},
    {"return_statement",               emit_return_statement_handler_code},
    {"string_tokenizer_loop",          emit_tokenizer_loop_handler_code},
    {"variable_declaration",           emit_variable_decl_handler_code},
    {NULL, NULL}
};

/* ── C bootstrap: main dispatcher loop ──────────────────────────────── */
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
