// SPDX-License-Identifier: Apache-2.0
// enforce.h — structural enforcement for spec2c compiler source

#ifndef ENFORCE_H
#define ENFORCE_H

#include <stddef.h>
void enforce_all_source_code_rules(const char *srcdir);
void display_current_source_structure_report(const char *srcdir);

/* ── shared helpers exported from enforce-naming-whitelist-and-validation.c ─── */
int  match_header_against_include_whitelist(const char *hdr);
void validate_name_against_soul_rules(const char *what, const char *name, const char *fp);
void read_allowed_names_from_file(const char *srcdir);
int  return_total_count_allowed_names(void);
const char *get_allowed_name_from_whitelist(int index);
void read_banned_patterns_from_file(const char *srcdir);
extern int banned_patterns_count;
extern char banned_patterns[32][64];
typedef struct { int depth, in_str, in_char, in_comment; } brace_state_t;
void clear_brace_tracking_for_function(brace_state_t *state);
void count_open_close_brace_pairs(const char *line, brace_state_t *state);

typedef struct {
    char name[128];
    char file[256];
} fn_entry_t;
typedef struct {
    char name[64];
    int count;
} inc_entry_t;
void pull_function_name_from_definition(const char *line, char *out, size_t sz);
int  check_for_banned_keyword_pattern(const char *line);
int  detect_hardcoded_file_path_string(const char *line);
int  match_name_against_stdlib_list(const char *name);
void verify_ipm_names_across_sources(const char *srcdir);
void load_bootstrap_whitelist_from_disk(const char *srcdir);
void load_non_source_file_allowlist(const char *srcdir);
int check_non_source_file_allowlist(const char *name);
int  match_name_against_bootstrap_list(const char *basename);
void enforce_bootstrap_code_freeze_check(const char *srcdir);

#endif




int match_type_against_strict_whitelist(const char *t);
void check_single_file_for_violations(const char *sub, int is_c, int is_source, fn_entry_t *fns, int *fn_qty, inc_entry_t *incs, int *inc_qty);
int match_source_code_header_filename(const char *name);
