// SPDX-License-Identifier: Apache-2.0
// ipm_builtins.h — runtime library declarations for spec2c-generated code
#ifndef IPM_BUILTINS_H
#define IPM_BUILTINS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <sys/types.h>

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
string read_entire_file_into_string(const char *path);
void   write_text_string_into_file(const char *path, const char *content);

/* JSON */
json_object* convert_text_into_json_object(string content);

/* Hash table (sorted array — deterministic iteration) */
subst_table* allocate_and_init_hash_table(void);
void         hash_table_insert_key_value(subst_table *table, const char *key, const char *value);
char*      hash_table_lookup_key_value(const subst_table *table, const char *key);
void         hash_table_free_all_entries(subst_table *table);
int          hash_table_count_all_entries(const subst_table *table);

/* String substitution */
string apply_substitution_against_raw_text(const char *template_str, const subst_table *table);

/* String buffer (append-to-memory, flush once — deterministic codegen) */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} string_buffer;
string_buffer* create_empty_growable_string_buffer(void);
void append_text_into_string_buffer(string_buffer *buf, const char *str);
void write_string_buffer_into_file(string_buffer *buf, const char *path);
void flush_string_buffer_into_stdout(string_buffer *buf);
void free_allocated_string_buffer_memory(string_buffer *buf);

/* Error handling — structured JSON output */
void terminate_with_json_error_output(const char *function_name, const char *instruction_index_str, const char *error_msg, const char *fix_hint);
void builtin_fatal_error_and_exit(const char *msg);
void print_error_into_stderr_output(const char *msg);
void terminate_with_status_return_code(int code);

/* Type mapping */
const char* resolve_spec_type_into_lang(const char *type_name);

/* Regex */
int match_pattern_against_text_string(const char *text, const char *pattern);

/* CLI argument access (set by auto-generated main) */
extern int g_argc;
extern char **g_argv;
int get_argument_count_from_global(void);
char* get_argument_value_from_global(int index);

/* Safe exec wrappers (replace system/popen) */
int   launch_command_with_argument_array(char *const argv[]);
FILE *execute_command_capture_stdout_pipe(char *const argv[], pid_t *out_pid);
int   close_pipe_await_child_finish(FILE *fp, pid_t pid);

#endif /* IPM_BUILTINS_H */
