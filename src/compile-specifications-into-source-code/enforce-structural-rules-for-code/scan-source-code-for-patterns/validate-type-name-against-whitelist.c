#include <string.h>
int match_type_against_strict_whitelist(const char *t) {
    return !strcmp(t, "u8") || !strcmp(t, "u32") || !strcmp(t, "u64") ||
           !strcmp(t, "i32") || !strcmp(t, "i64") ||
           !strcmp(t, "u8_ptr") || !strcmp(t, "ptr") ||
           !strcmp(t, "str") || !strcmp(t, "cjson") ||
           !strcmp(t, "string") || !strcmp(t, "json_object") || !strcmp(t, "json_array") ||
           !strcmp(t, "void") || !strcmp(t, "file_handle") ||
           !strcmp(t, "float") || !strcmp(t, "boolean") ||
           !strcmp(t, "int") || !strcmp(t, "db_handle") ||
           !strcmp(t, "subst_table") || !strcmp(t, "string_buffer");
}
