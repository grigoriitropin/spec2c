// SPDX-License-Identifier: Apache-2.0
#include "../shared-type-declarations-across-modules/share-type-definitions-across-files.h"
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
