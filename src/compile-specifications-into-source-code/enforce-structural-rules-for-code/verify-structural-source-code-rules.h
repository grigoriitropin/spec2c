// SPDX-License-Identifier: Apache-2.0
// enforce.h — structural enforcement for spec2c compiler source

#ifndef ENFORCE_H
#define ENFORCE_H

void enforce_all_source_code_rules(const char *srcdir);
void display_current_source_structure_report(const char *srcdir);

/* ── shared helpers exported from enforce-naming-whitelist-and-validation.c ─── */
int  match_header_against_include_whitelist(const char *hdr);
void validate_name_against_soul_rules(const char *what, const char *name, const char *fp);
void read_allowed_names_from_file(const char *srcdir);
int  return_total_count_allowed_names(void);
const char *get_allowed_name_from_whitelist(int index);
int  check_for_banned_keyword_pattern(const char *line);
void read_banned_patterns_from_file(const char *srcdir);

#endif
