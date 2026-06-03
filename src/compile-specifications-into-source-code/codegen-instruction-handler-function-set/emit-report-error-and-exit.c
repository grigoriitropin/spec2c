// SPDX-License-Identifier: Apache-2.0
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

int emit_report_error_then_exit(cJSON *inst, FILE *out) {
    cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
    const char *v[3] = {"", "violation", "fix the issue"};
    if (args && cJSON_IsArray(args)) {
        for (int ai = 0; ai < cJSON_GetArraySize(args) && ai < 3; ai++) {
            cJSON *a = cJSON_GetArrayItem(args, ai);
            if (cJSON_IsString(a)) v[ai] = a->valuestring;
            else if (cJSON_IsObject(a)) v[ai] = extract_json_field_string_value(a, "value");
        }
    }
    fprintf(out, "  fprintf(stderr, \"spec2c: SOUL §7: %%s — %%s\\n  → %%s\\n\", %s, \"%s\", \"%s\");\n", v[0], v[1], v[2]);
    fprintf(out, "  exit(1);\n");
    return 1;
}

void emit_iteration_loop_with_count(cJSON *inst, FILE *out, int indent, const char *rt) {
    const char *cv = extract_json_field_string_value(inst, "counter_variable");
    const char *lv = extract_json_field_string_value(inst, "limit_variable");
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
    if (!cv[0] || !lv[0]) return;
    fprintf(out, "  for (int %s = 0; %s < %s; %s++) {\n", cv, cv, lv, cv);
    if (body) generate_code_from_ast_instructions(body, out, indent + 2, rt);
    fprintf(out, "  }\n");
}
