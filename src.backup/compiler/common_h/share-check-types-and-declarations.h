// SPDX-License-Identifier: Apache-2.0
#ifndef SHARE_CHECK_TYPES_AND_DECLARATIONS_H
#define SHARE_CHECK_TYPES_AND_DECLARATIONS_H

#include <stddef.h>
#include <stdint.h>

#ifndef PATH_MAX_SZ
#define PATH_MAX_SZ 4096
#endif

typedef enum { SEV_INFO, SEV_WARNING, SEV_ERROR } severity_t;

typedef struct {
    const char *check_id;
    const char *pattern_id;
    severity_t  severity;
    int         line;
    int         col;
    const char *source_line;
    const char *message;
    const char *suggestion;
    int         in_scaffold;
} deviation_t;

typedef struct {
    deviation_t *items;
    int          count;
    int          cap;
} deviation_list_t;

typedef struct {
    const char  *id;
    const char  *severity_str;
    severity_t   severity;
    const char  *message;
    const char  *suggestion;
    int          nidentifiers;
    const char **identifiers;
    int          nforbidden_strings;
    const char **forbidden_strings;
    int          nsql_patterns;
    const char **sql_patterns;
} pattern_def_t;

void   initialize_deviation_list_structure(deviation_list_t *dl);
void   append_deviation_to_report_list(deviation_list_t *dl, const char *check_id, const char *pattern_id,
                    severity_t sev, int line, int col, const char *source_line,
                    const char *msg, const char *sug, int in_scaffold);
const char *convert_severity_into_readable_string(severity_t s);
severity_t  parse_severity_from_config_string(const char *s);

void   match_identifier_against_ast_grep(const pattern_def_t *pat, const char *identifier,
                    const char *sg_cmd, const char *file_path, const char *file_content,
                    deviation_list_t *dl);
void   match_string_against_file_content(const pattern_def_t *pat, const char *file_path,
                    const char *file_content, deviation_list_t *dl);
void   scan_for_scaffold_marker_deviations(const char *file_content, const char *spec_name,
                    deviation_list_t *dl);
int    read_pattern_definitions_from_json(const char *path, pattern_def_t **patterns_out, int *npatterns_out);
void   write_conformance_report_to_output(const deviation_list_t *dl, const char *file_path,
                    int scaffold_ok, int error_count, int warning_count);

char        *read_entire_text_into_memory(const char *path);
int          launch_command_with_argument_list(char *const argv[]);
FILE        *execute_command_capture_stdout_pipe(char *const argv[], pid_t *out_pid);
int          close_pipe_await_child_finish(FILE *fp, pid_t pid);
_Noreturn void terminate_with_error_message_output(const char *msg);

char *escape_string_for_shell_command(const char *s);
int   verify_ast_grep_command_exists(void);
const char *find_ast_grep_executable_path(void);
char *execute_ast_grep_with_pattern(const char *sg_cmd, const char *file_path, const char *pattern);

#endif
