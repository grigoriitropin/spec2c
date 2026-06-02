// SPDX-License-Identifier: Apache-2.0
// ipm_builtins.h — runtime library declarations for spec2c-generated code
#ifndef IPM_BUILTINS_H
#define IPM_BUILTINS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef char* string;
typedef cJSON json_object;

typedef struct {
    char *key;
    char *value;
} subst_entry;

typedef struct {
    subst_entry *entries;
    int count;
    int capacity;
} subst_table;

/* I/O */
string read_file_to_string(const char *path);
void   write_string_to_file(const char *path, const char *content);

/* JSON */
json_object* parse_json_string(string content);

/* Hash table (sorted array — deterministic iteration) */
subst_table* create_hash_table(void);
void         hash_table_insert(subst_table *table, const char *key, const char *value);
char*      hash_table_lookup(const subst_table *table, const char *key);
void         hash_table_free(subst_table *table);
int          hash_table_count(const subst_table *table);

/* String substitution */
string string_substitute(const char *template_str, const subst_table *table);

/* String buffer (append-to-memory, flush once — deterministic codegen) */
typedef struct { char *data; size_t len; size_t cap; } string_buffer;
string_buffer* create_string_buffer(void);
void append_to_buffer(string_buffer *buf, const char *str);
void write_buffer_to_file(string_buffer *buf, const char *path);
void flush_buffer_to_stdout(string_buffer *buf);
void free_string_buffer(string_buffer *buf);

/* Error handling — structured JSON output */
void json_die(const char *function_name, const char *instruction_index_str, const char *error_msg, const char *fix_hint);
void die_builtin(const char *msg);
void print_error_to_stderr(const char *msg);
void exit_process(int code);

/* Type mapping */
char* vartype_to_c(const char *type_name);

/* Regex */
int regex_match(const char *text, const char *pattern);

#endif /* IPM_BUILTINS_H */
