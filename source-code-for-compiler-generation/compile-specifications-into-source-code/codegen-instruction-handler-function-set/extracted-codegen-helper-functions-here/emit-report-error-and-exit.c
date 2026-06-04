// SPDX-License-Identifier: Apache-2.0
#include "../../shared-type-declarations-across-modules/share-type-definitions-across-files.h"

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

void tokenize_string_into_slice_loop(cJSON *inst, FILE *out, int indent, const char *rt) {
    const char *src = extract_json_field_string_value(inst, "source_string");
    const char *sep = extract_json_field_string_value(inst, "separator");
    const char *tok = extract_json_field_string_value(inst, "token_variable");
    const char *len = extract_json_field_string_value(inst, "length_variable");
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body");
    (void)rt;
    (void)indent;
    if (!src[0] || !sep[0]) return;
    fprintf(out, "  {\n");
    fprintf(out, "    const char *_zsrc = %s;\n", src);
    fprintf(out, "    while (*_zsrc) {\n");
    fprintf(out, "      const char *_zend = _zsrc;\n");
    fprintf(out, "      while (*_zend && *_zend != '%s') _zend++;\n", sep);
    if (len[0]) fprintf(out, "      int %s = _zend - _zsrc;\n", len);
    if (tok[0]) fprintf(out, "      const char *%s = _zsrc;\n", tok);
    {
        const char *prev_slice = current_slice_length_variable;
        current_slice_length_variable = len;
        if (body) generate_code_from_ast_instructions(body, out, 3, rt);
        current_slice_length_variable = prev_slice;
    }
    fprintf(out, "      if (!*_zend) break;\n");
    fprintf(out, "      _zsrc = _zend + 1;\n");
    fprintf(out, "    }\n");
    fprintf(out, "  }\n");
}

const char *search_variable_type_recursive_ast(cJSON *node, const char *var_name) {
    if (!node) return NULL;
    if (cJSON_IsObject(node)) {
        cJSON *it = cJSON_GetObjectItemCaseSensitive(node, "instruction_type");
        if (it && cJSON_IsString(it)) {
            if (!strcmp(it->valuestring, "variable_declaration")) {
                cJSON *vn = cJSON_GetObjectItemCaseSensitive(node, "variable_name");
                if (vn && cJSON_IsString(vn) && !strcmp(vn->valuestring, var_name)) {
                    cJSON *vt = cJSON_GetObjectItemCaseSensitive(node, "variable_type");
                    if (vt && cJSON_IsString(vt)) return vt->valuestring;
                }
            } else if (!strcmp(it->valuestring, "read_file_content")) {
                cJSON *rv = cJSON_GetObjectItemCaseSensitive(node, "result_variable");
                if (rv && cJSON_IsString(rv) && !strcmp(rv->valuestring, var_name)) {
                    return "slice";
                }
            } else if (!strcmp(it->valuestring, "function_invocation")) {
                cJSON *rv = cJSON_GetObjectItemCaseSensitive(node, "result_assignment_variable");
                if (rv && cJSON_IsString(rv) && !strcmp(rv->valuestring, var_name)) {
                    cJSON *rt = cJSON_GetObjectItemCaseSensitive(node, "result_type");
                    if (rt && cJSON_IsString(rt)) return rt->valuestring;
                }
            }
        }
        cJSON *child = node->child;
        while (child) {
            const char *res = search_variable_type_recursive_ast(child, var_name);
            if (res) return res;
            child = child->next;
        }
    } else if (cJSON_IsArray(node)) {
        cJSON *child = node->child;
        while (child) {
            const char *res = search_variable_type_recursive_ast(child, var_name);
            if (res) return res;
            child = child->next;
        }
    }
    return NULL;
}

const char *lookup_variable_type_current_function(const char *var_name) {
    if (!current_function_definition_ast) return NULL;
    cJSON *params = cJSON_GetObjectItemCaseSensitive(current_function_definition_ast, "parameter_definitions");
    if (params && cJSON_IsArray(params)) {
        for (int i = 0; i < cJSON_GetArraySize(params); i++) {
            cJSON *param = cJSON_GetArrayItem(params, i);
            cJSON *pn = cJSON_GetObjectItemCaseSensitive(param, "parameter_name");
            if (pn && cJSON_IsString(pn) && !strcmp(pn->valuestring, var_name)) {
                cJSON *pt = cJSON_GetObjectItemCaseSensitive(param, "parameter_type");
                if (pt && cJSON_IsString(pt)) return pt->valuestring;
            }
        }
    }
    cJSON *body = cJSON_GetObjectItemCaseSensitive(current_function_definition_ast, "execution_instructions");
    return search_variable_type_recursive_ast(body, var_name);
}

void emit_iteration_over_bytes_body(cJSON *inst, FILE *out, int indent, const char *return_type) {
    const char *col = extract_json_field_string_value(inst, "collection");
    const char *iv  = extract_json_field_string_value(inst, "item_variable");
    const char *xv  = extract_json_field_string_value(inst, "index_variable");
    cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body");
    if (!col[0]) return;
    const char *vtype = lookup_variable_type_current_function(col);
    if (vtype && (!strcmp(vtype, "slice") || !strcmp(vtype, "Slice"))) {
        fprintf(out, "  Slice _sl = %s;\n", col);
    } else {
        fprintf(out, "  Slice _sl;\n");
        fprintf(out, "  _sl.data = (const uint8_t *)(%s);\n", col);
        fprintf(out, "  _sl.len = strlen(%s);\n", col);
    }
    fprintf(out, "  for (uint32_t %s = 0; %s < _sl.len; %s++) {\n", xv, xv, xv);
    fprintf(out, "    uint8_t %s = _sl.data[%s];\n", iv, xv);
    if (body) generate_code_from_ast_instructions(body, out, indent + 2, return_type);
    fprintf(out, "  }\n");
}


