// SPDX-License-Identifier: Apache-2.0
// ipm_builtins — I/O, JSON, error, regex, type mapping (split part 1)
#include "runtime-for-generated-ipm-code.h"
#include <unistd.h>
#include <regex.h>

string read_entire_file_into_string(const char *path) {
    FILE *f = (!path || !path[0]) ? stdin : fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) { if (f != stdin) fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)size, f);
    buffer[n] = '\0';
    if (f != stdin) fclose(f);
    return buffer;
}

void write_text_string_into_file(const char *path, const char *content) {
    if (!path || !content) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

json_object* convert_text_into_json_object(string content) {
    if (!content) return NULL;
    return cJSON_Parse(content);
}

void terminate_with_json_error_output(const char *function_name, const char *instruction_index_str,
              const char *error_msg, const char *fix_hint) {
    cJSON *r = cJSON_CreateObject();
    if (!r) { fprintf(stderr, "{\"ok\":false,\"error\":\"cJSON alloc failed\"}\n"); exit(1); }
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", error_msg ? error_msg : "unknown error");
    if (function_name && function_name[0])
        cJSON_AddStringToObject(r, "function", function_name);
    if (instruction_index_str && instruction_index_str[0])
        cJSON_AddNumberToObject(r, "instruction_index", atoi(instruction_index_str));
    if (fix_hint && fix_hint[0])
        cJSON_AddStringToObject(r, "fix_hint", fix_hint);
    char *s = cJSON_PrintUnformatted(r);
    if (s) { fprintf(stderr, "%s\n", s); free(s); }
    cJSON_Delete(r);
    exit(1);
}

void builtin_fatal_error_and_exit(const char *msg) {
    fprintf(stderr, "{\"ok\":false,\"error\":\"%s\"}\n", msg ? msg : "unknown error");
    exit(1);
}

void print_error_into_stderr_output(const char *msg) {
    fprintf(stderr, "error: %s\n", msg ? msg : "unknown");
}

void terminate_with_status_return_code(int code) {
    exit(code);
}

int match_pattern_against_text_string(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    regex_t regex;
    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB)) return 0;
    int rc = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);
    return rc == 0 ? 1 : 0;
}

const char *resolve_spec_type_into_lang(const char *t) {
    if (!t) return "void *";
    if (!strcmp(t, "void")) return "void";
    if (!strcmp(t, "string")) return "char *";
    if (!strcmp(t, "int")) return "int";
    if (!strcmp(t, "float")) return "float";
    if (!strcmp(t, "boolean")) return "int";
    if (!strcmp(t, "json_object")) return "cJSON *";
    if (!strcmp(t, "json_array")) return "cJSON *";
    if (!strcmp(t, "db_handle")) return "struct vehir_db *";
    if (!strcmp(t, "subst_table")) return "subst_table *";
    if (!strcmp(t, "string_buffer")) return "string_buffer *";
    return "void *";
}
