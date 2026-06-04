// SPDX-License-Identifier: Apache-2.0
// enforce.h — structural enforcement for spec2c compiler source

#ifndef ENFORCE_H
#define ENFORCE_H

#include <stddef.h>
#include <string.h>
#define MAX_INCLUDES 128
#define match_source_code_header_filename(name) (strlen(name) > 2 && (!strcmp((name) + strlen(name) - 2, ".c") || !strcmp((name) + strlen(name) - 2, ".h")))
typedef enum {
    ERR_FILE_TOO_LONG,
    ERR_TOO_MANY_FUNCTIONS,
    ERR_FUNCTION_TOO_LONG,
    ERR_TOO_MANY_FILES_IN_DIR,
    ERR_BANNED_PATTERN,
    ERR_HARDCODED_PATH,
    ERR_HEADER_NOT_IN_WHITELIST,
    ERR_HEADER_INCLUDED_TOO_OFTEN,
    ERR_DEAD_CODE,
    ERR_MAIN_COUNT,
    ERR_FLAG_NOT_IN_HELP,
    ERR_LINE_TOO_DENSE,
    ERR_NOT_IN_BOOTSTRAP_WHITELIST
} enforce_err_t;
extern int lint_mode;
extern int lint_errors;
void enforce_all_source_code_rules(const char *srcdir);
void display_current_source_structure_report(const char *srcdir);

/* ── shared helpers exported from enforce-naming-whitelist-and-validation.c ─── */
int  match_header_against_include_whitelist(const char *hdr);
void validate_name_against_soul_rules(const char *what, const char *name, const char *fp);
void read_allowed_names_from_file(const char *srcdir);
void read_banned_patterns_from_file(const char *srcdir);
extern int banned_patterns_count;
extern char banned_patterns[32][64];
typedef struct {
    int depth, in_str, in_char, in_comment;
} brace_state_t;

typedef struct {
    char name[128];
    char file[256];
} fn_entry_t;

typedef struct {
    char name[64];
    int count;
} inc_entry_t;

void check_single_file_for_violations(const char *sub, int is_c, int is_source,
    fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty);
void search_for_unused_function_code(fn_entry_t *fns, int fn_qty, const char *srcdir);
void scan_source_for_undocumented_flags(const char *srcdir);
void skip_root_files_when_scanning(const char *srcdir);
int  check_main_count_exemption_rule(const char *path);
void check_include_headers_for_file(const char *sub);
void report_fatal_error_and_exit(const char *msg);
void report_violation_with_actionable_hint(enforce_err_t code, const char *a1, int v1, int v2, const char *a2);

void clear_brace_tracking_for_function(brace_state_t *state);
void count_open_close_brace_pairs(const char *line, brace_state_t *state);
void pull_function_name_from_definition(const char *line, char *out, size_t sz);
int  check_for_banned_keyword_pattern(const char *line);
int  detect_hardcoded_file_path_string(const char *line, const char *filepath);
int  validate_file_stem_with_dfa(const char *stem, const char *fullname, const char *path);
int  count_stem_tokens_lowercase_bytewise(const char *stem, const char *fullname, const char *path);
void verify_ipm_names_across_sources(const char *srcdir);
void load_bootstrap_whitelist_from_disk(const char *srcdir);
void load_non_source_file_allowlist(const char *srcdir);
int check_non_source_file_allowlist(const char *name);
int match_name_against_bootstrap_list(const char *basename);
void enforce_bootstrap_code_freeze_check(const char *srcdir);
int  load_operator_integrity_manifest_file(const char *srcdir, char **out, long *out_len);
void load_manifest_paths_for_exemption(const char *srcdir);
void load_operator_signed_exemption_table(const char *srcdir);
const char *match_name_against_exemption_table(const char *name);
int  match_path_against_integrity_manifest(const char *relpath);
int check_name_against_allowed_whitelist(const char *name);

/* directories exempt from main() counting (names only, no slashes) */
static const char *main_exempt_directory_names[] __attribute__((unused)) = {
    "enforce-structural-rules-for-code",
    "verify-source-against-soul-patterns",
    "helper-standalone-executables-for-spec2c",
    NULL
};

#endif




int match_type_against_strict_whitelist(const char *t);
