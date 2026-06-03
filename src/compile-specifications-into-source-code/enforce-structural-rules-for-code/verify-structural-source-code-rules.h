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
void read_banned_patterns_from_file(const char *srcdir);
extern int banned_patterns_count;
extern char banned_patterns[32][64];
typedef struct { int depth, in_str, in_char, in_comment; } brace_state_t;
void clear_brace_tracking_for_function(brace_state_t *state);
void count_open_close_brace_pairs(const char *line, brace_state_t *state);
void pull_function_name_from_definition(const char *line, char *out, size_t sz);
int  check_for_banned_keyword_pattern(const char *line);
int  detect_hardcoded_file_path_string(const char *line);
int  validate_file_stem_with_dfa(const char *stem, const char *fullname, const char *path);
void verify_ipm_names_across_sources(const char *srcdir);
void load_bootstrap_whitelist_from_disk(const char *srcdir);
void load_non_source_file_allowlist(const char *srcdir);
int check_non_source_file_allowlist(const char *name);
int  match_name_against_bootstrap_list(const char *basename);
void enforce_bootstrap_code_freeze_check(const char *srcdir);
void load_operator_signed_exemption_table(const char *srcdir);
const char *match_name_against_exemption_table(const char *name);

#endif




int match_type_against_strict_whitelist(const char *t);
