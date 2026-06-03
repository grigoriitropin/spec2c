#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>


// @ipm:spec-handler-definition-compendium-one:emit_json_field_access_handler
void emit_json_field_access_handler(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *vn = cJSON_GetObjectItemCaseSensitive(inst,"variable_name") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_name")->valuestring : "";
  const char *vt = cJSON_GetObjectItemCaseSensitive(inst,"variable_type") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_type")->valuestring : "";
  const char *so = cJSON_GetObjectItemCaseSensitive(inst,"source_object") ? cJSON_GetObjectItemCaseSensitive(inst,"source_object")->valuestring : "";
  const char *fn = cJSON_GetObjectItemCaseSensitive(inst,"field_name") ? cJSON_GetObjectItemCaseSensitive(inst,"field_name")->valuestring : "";
  if (vn != NULL) {
    if (strcmp(vt, "string") == 0) {
      fprintf(out, "const char *%s = cJSON_GetObjectItemCaseSensitive(%s,\"%s\") ? cJSON_GetObjectItemCaseSensitive(%s,\"%s\")->valuestring : \"\";\n", vn, so, fn, so, fn);
    } else {
      fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
    }
  } else {
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_return_statement_handler_code
void emit_return_statement_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  fprintf(out, "return;\n");
}

// @ipm:spec-handler-definition-compendium-one:emit_function_invocation_handler_code
void emit_function_invocation_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *fn = cJSON_GetObjectItemCaseSensitive(inst,"invocation_name") ? cJSON_GetObjectItemCaseSensitive(inst,"invocation_name")->valuestring : "";
  const char *rv = cJSON_GetObjectItemCaseSensitive(inst,"result_assignment_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"result_assignment_variable")->valuestring : "";
  if (rv != NULL) {
    fprintf(out, "int %s = %s();\n", rv, fn);
  } else {
    fprintf(out, "%s();\n", fn);
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_iterate_collection_handler_code
void emit_iterate_collection_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *col = cJSON_GetObjectItemCaseSensitive(inst,"collection_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"collection_variable")->valuestring : "";
  const char *itm = cJSON_GetObjectItemCaseSensitive(inst,"item_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"item_variable")->valuestring : "";
  if (col != NULL) {
    fprintf(out, "for (int _i_%s = 0; _i_%s < cJSON_GetArraySize(%s); _i_%s++) {\n", col, col, col, col);
    fprintf(out, "  cJSON *%s = cJSON_GetArrayItem(%s, _i_%s);\n", itm, col, col);
    fprintf(out, "  cJSON *body_%s = cJSON_GetObjectItemCaseSensitive(inst, \"body_instructions\");\n", col);
    fprintf(out, "  if (body_%s) generate_code_from_ast_instructions(body_%s, out, indent + 1, return_type);\n", col, col);
    fprintf(out, "}\n");
  } else {
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_object_keys_iterate_handler
void emit_object_keys_iterate_handler(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *col = cJSON_GetObjectItemCaseSensitive(inst,"collection_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"collection_variable")->valuestring : "";
  const char *key = cJSON_GetObjectItemCaseSensitive(inst,"key_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"key_variable")->valuestring : "";
  const char *val = cJSON_GetObjectItemCaseSensitive(inst,"value_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"value_variable")->valuestring : "";
  if (col != NULL) {
    fprintf(out, "cJSON_ArrayForEach(%s, %s) {\n", val, col);
    if (key != NULL) {
      fprintf(out, "  const char *%s = %s->string;\n", key, val);
    } else {
    }
    fprintf(out, "  cJSON *body_%s = cJSON_GetObjectItemCaseSensitive(inst, \"body_instructions\");\n", col);
    fprintf(out, "  if (body_%s) generate_code_from_ast_instructions(body_%s, out, indent + 1, return_type);\n", col, col);
    fprintf(out, "}\n");
  } else {
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_tokenizer_loop_handler_code
void emit_tokenizer_loop_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *src = cJSON_GetObjectItemCaseSensitive(inst,"source_string") ? cJSON_GetObjectItemCaseSensitive(inst,"source_string")->valuestring : "";
  const char *sep = cJSON_GetObjectItemCaseSensitive(inst,"separator") ? cJSON_GetObjectItemCaseSensitive(inst,"separator")->valuestring : "";
  const char *tok = cJSON_GetObjectItemCaseSensitive(inst,"token_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"token_variable")->valuestring : "";
  if (src != NULL) {
    fprintf(out, "{\n");
    fprintf(out, "  char _buf[256]; snprintf(_buf, sizeof(_buf), \"%%s\", %s);\n", src);
    fprintf(out, "  char *_save;\n");
    fprintf(out, "  for (char *_t = strtok_r(_buf, %s, &_save); _t; _t = strtok_r(NULL, %s, &_save)) {\n", sep, sep);
    fprintf(out, "    const char *%s = _t;\n", tok);
    fprintf(out, "    cJSON *body_%s = cJSON_GetObjectItemCaseSensitive(inst, \"body_instructions\");\n", src);
    fprintf(out, "    if (body_%s) generate_code_from_ast_instructions(body_%s, out, indent + 1, return_type);\n", src, src);
    fprintf(out, "  }\n");
    fprintf(out, "}\n");
  } else {
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_count_loop_handler_code
void emit_count_loop_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *cv = cJSON_GetObjectItemCaseSensitive(inst,"counter_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"counter_variable")->valuestring : "";
  const char *lv = cJSON_GetObjectItemCaseSensitive(inst,"limit_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"limit_variable")->valuestring : "";
  if (cv != NULL) {
    fprintf(out, "for (int %s = 0; %s < %s; %s++) {\n", cv, cv, lv, cv);
    fprintf(out, "  cJSON *body_%s = cJSON_GetObjectItemCaseSensitive(inst, \"body_instructions\");\n", cv);
    fprintf(out, "  if (body_%s) generate_code_from_ast_instructions(body_%s, out, indent + 1, return_type);\n", cv, cv);
    fprintf(out, "}\n");
  } else {
  }
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_branch_condition_handler_code
void emit_branch_condition_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *op = cJSON_GetObjectItemCaseSensitive(inst,"condition_operation") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_operation")->valuestring : "";
  const char *tgt = cJSON_GetObjectItemCaseSensitive(inst,"condition_target") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_target")->valuestring : "";
  const char *cv = cJSON_GetObjectItemCaseSensitive(inst,"condition_value") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_value")->valuestring : "";
  if (strcmp(op, "key_exists") == 0) {
    fprintf(out, "if (cJSON_HasObjectItem(%s, \"%s\")) {\n", tgt, cv);
  } else {
    if (strcmp(op, "is_not_null") == 0) {
      fprintf(out, "if (%s != NULL) {\n", tgt);
    } else {
      fprintf(out, "if (strcmp(%s, \"%s\") == 0) {\n", tgt, cv);
    }
  }
  fprintf(out, "  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, \"branch_on_success\");\n");
  fprintf(out, "  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);\n");
  fprintf(out, "} else {\n");
  fprintf(out, "  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, \"branch_on_failure\");\n");
  fprintf(out, "  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);\n");
  fprintf(out, "}\n");
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_variable_decl_handler_code
void emit_variable_decl_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
    (void)indent; (void)return_type;
  const char *vn = cJSON_GetObjectItemCaseSensitive(inst,"variable_name") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_name")->valuestring : "";
  const char *vt = cJSON_GetObjectItemCaseSensitive(inst,"variable_type") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_type")->valuestring : "";
  const char *op = cJSON_GetObjectItemCaseSensitive(inst,"assignment_operation") ? cJSON_GetObjectItemCaseSensitive(inst,"assignment_operation")->valuestring : "";
  const char *src = cJSON_GetObjectItemCaseSensitive(inst,"source_target") ? cJSON_GetObjectItemCaseSensitive(inst,"source_target")->valuestring : "";
  if (vn != NULL) {
    fprintf(out, "  %s %s = %s(%s);\n", vt, vn, op, src);
  } else {
  }
  return;
}

