// SPDX-License-Identifier: Apache-2.0
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

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
