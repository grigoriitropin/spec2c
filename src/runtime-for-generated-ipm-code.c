// SPDX-License-Identifier: Apache-2.0
// ipm_builtins.c — software runtime library for spec2c-generated code
#include <stddef.h>

#pragma weak validate_file_stem_naming_dfa
int validate_file_stem_naming_dfa(const char *name_arg) {
    (void)name_arg;
    return 0;
}

#pragma weak find_every_main_function_block
int find_every_main_function_block(const char *path) {
    (void)path;
    return 0;
}

#pragma weak read_allowed_names_from_file
void read_allowed_names_from_file(const char *srcdir) {
    (void)srcdir;
}

#pragma weak read_banned_patterns_from_file
void read_banned_patterns_from_file(const char *srcdir) {
    (void)srcdir;
}

#pragma weak load_non_source_file_allowlist
void load_non_source_file_allowlist(const char *srcdir) {
    (void)srcdir;
}

#pragma weak load_bootstrap_whitelist_from_disk
void load_bootstrap_whitelist_from_disk(const char *srcdir) {
    (void)srcdir;
}

#pragma weak load_operator_signed_exemption_table
void load_operator_signed_exemption_table(const char *srcdir) {
    (void)srcdir;
}
__attribute__((weak)) void *malloc(size_t n) { (void)n; return (void*)0; }
