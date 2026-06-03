#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>



// @ipm:handler-file-input-output-scanning:emit_read_file_content_handler
void emit_read_file_content_handler(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *fp = cJSON_GetObjectItemCaseSensitive(inst,"file_path") ? cJSON_GetObjectItemCaseSensitive(inst,"file_path")->valuestring : "";
  const char *rv = cJSON_GetObjectItemCaseSensitive(inst,"result_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"result_variable")->valuestring : "";
  if (fp != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}

// @ipm:handler-file-input-output-scanning:emit_scan_directory_entries_handler
void emit_scan_directory_entries_handler(cJSON * inst, FILE * out, int indent, char * return_type) {
    (void)inst;
    (void)out;
    (void)indent;
    (void)return_type;
  const char *dp = cJSON_GetObjectItemCaseSensitive(inst,"directory_path") ? cJSON_GetObjectItemCaseSensitive(inst,"directory_path")->valuestring : "";
  const char *ev = cJSON_GetObjectItemCaseSensitive(inst,"entry_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"entry_variable")->valuestring : "";
  const char *nv = cJSON_GetObjectItemCaseSensitive(inst,"name_variable") ? cJSON_GetObjectItemCaseSensitive(inst,"name_variable")->valuestring : "";
  if (dp != NULL) {
  cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
  if (bon) generate_code_from_ast_instructions(bon, out, indent + 1, return_type);
} else {
  cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
  if (bof) generate_code_from_ast_instructions(bof, out, indent + 1, return_type);
}
  return;
}



