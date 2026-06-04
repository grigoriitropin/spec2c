// SPDX-License-Identifier: Apache-2.0
// runtime-for-generated-ipm-code.c — IPM runtime stubs (fail-CLOSED)
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#pragma weak validate_file_stem_naming_dfa
int validate_file_stem_naming_dfa(const char *name_arg) {
    (void)name_arg;
    fprintf(stderr, "FATAL: validate_file_stem_naming_dfa stub called — not implemented\n");
    exit(1);
}

#pragma weak find_every_main_function_block
int find_every_main_function_block(const char *path) {
    (void)path;
    fprintf(stderr, "FATAL: find_every_main_function_block stub called — not implemented\n");
    exit(1);
}

#pragma weak read_allowed_names_from_file
void read_allowed_names_from_file(const char *srcdir) {
    (void)srcdir;
    fprintf(stderr, "FATAL: read_allowed_names_from_file stub called — not implemented\n");
    exit(1);
}

#pragma weak read_banned_patterns_from_file
void read_banned_patterns_from_file(const char *srcdir) {
    (void)srcdir;
    fprintf(stderr, "FATAL: read_banned_patterns_from_file stub called — not implemented\n");
    exit(1);
}

#pragma weak load_non_source_file_allowlist
void load_non_source_file_allowlist(const char *srcdir) {
    (void)srcdir;
    fprintf(stderr, "FATAL: load_non_source_file_allowlist stub called — not implemented\n");
    exit(1);
}

#pragma weak load_bootstrap_whitelist_from_disk
void load_bootstrap_whitelist_from_disk(const char *srcdir) {
    (void)srcdir;
    fprintf(stderr, "FATAL: load_bootstrap_whitelist_from_disk stub called — not implemented\n");
    exit(1);
}

#pragma weak load_operator_signed_exemption_table
void load_operator_signed_exemption_table(const char *srcdir) {
    (void)srcdir;
    fprintf(stderr, "FATAL: load_operator_signed_exemption_table stub called — not implemented\n");
    exit(1);
}
