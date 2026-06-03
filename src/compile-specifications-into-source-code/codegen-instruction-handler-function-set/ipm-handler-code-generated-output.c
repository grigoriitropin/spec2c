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
  const char *vn = cJSON_GetObjectItemCaseSensitive(inst,"variable_name") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_name")->valuestring : "";
  const char *vt = cJSON_GetObjectItemCaseSensitive(inst,"variable_type") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_type")->valuestring : "";
  const char *so = cJSON_GetObjectItemCaseSensitive(inst,"source_object") ? cJSON_GetObjectItemCaseSensitive(inst,"source_object")->valuestring : "";
  const char *fn = cJSON_GetObjectItemCaseSensitive(inst,"field_name") ? cJSON_GetObjectItemCaseSensitive(inst,"field_name")->valuestring : "";
  if (vn != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_return_statement_handler_code
void emit_return_statement_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  fprintf(out, "return 0;\n");
}

// @ipm:spec-handler-definition-compendium-one:emit_function_invocation_handler_code
void emit_function_invocation_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *fn = cJSON_GetObjectItemCaseSensitive(inst,"invocation_name") ? cJSON_GetObjectItemCaseSensitive(inst,"invocation_name")->valuestring : "";
  const char *rv = cJSON_GetObjectItemCaseSensitive(inst,"result_assignment_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"result_assignment_variable")->valuestring : "";
  if (rv != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_iterate_collection_handler_code
void emit_iterate_collection_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *col = cJSON_GetObjectItemCaseSensitive(inst,"collection_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"collection_variable")->valuestring : "";
  const char *itm = cJSON_GetObjectItemCaseSensitive(inst,"item_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"item_variable")->valuestring : "";
  if (col != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_object_keys_iterate_handler
void emit_object_keys_iterate_handler(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *col = cJSON_GetObjectItemCaseSensitive(inst,"collection_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"collection_variable")->valuestring : "";
  const char *key = cJSON_GetObjectItemCaseSensitive(inst,"key_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"key_variable")->valuestring : "";
  const char *val = cJSON_GetObjectItemCaseSensitive(inst,"value_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"value_variable")->valuestring : "";
  if (col != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_tokenizer_loop_handler_code
void emit_tokenizer_loop_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *src = cJSON_GetObjectItemCaseSensitive(inst,"source_string") ? cJSON_GetObjectItemCaseSensitive(inst,"source_string")->valuestring : "";
  const char *sep = cJSON_GetObjectItemCaseSensitive(inst,"separator") ? cJSON_GetObjectItemCaseSensitive(inst,"separator")->valuestring : "";
  const char *tok = cJSON_GetObjectItemCaseSensitive(inst,"token_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"token_variable")->valuestring : "";
  if (src != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_count_loop_handler_code
void emit_count_loop_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *cv = cJSON_GetObjectItemCaseSensitive(inst,"counter_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"counter_variable")->valuestring : "";
  const char *lv = cJSON_GetObjectItemCaseSensitive(inst,"limit_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"limit_variable")->valuestring : "";
  if (cv != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:spec-handler-definition-compendium-one:emit_branch_condition_handler_code
void emit_branch_condition_handler_code(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *op = cJSON_GetObjectItemCaseSensitive(inst,"condition_operation") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_operation")->valuestring : "";
  const char *tgt = cJSON_GetObjectItemCaseSensitive(inst,"condition_target") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_target")->valuestring : "";
  const char *cv = cJSON_GetObjectItemCaseSensitive(inst,"condition_value") ? cJSON_GetObjectItemCaseSensitive(inst,"condition_value")->valuestring : "";
  if (strcmp(op, "key_exists") == 0) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
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
  const char *vn = cJSON_GetObjectItemCaseSensitive(inst,"variable_name") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_name")->valuestring : "";
  const char *vt = cJSON_GetObjectItemCaseSensitive(inst,"variable_type") ? cJSON_GetObjectItemCaseSensitive(inst,"variable_type")->valuestring : "";
  const char *op = cJSON_GetObjectItemCaseSensitive(inst,"assignment_operation") ? cJSON_GetObjectItemCaseSensitive(inst,"assignment_operation")->valuestring : "";
  const char *src = cJSON_GetObjectItemCaseSensitive(inst,"source_target") ? cJSON_GetObjectItemCaseSensitive(inst,"source_target")->valuestring : "";
  if (vn != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}



