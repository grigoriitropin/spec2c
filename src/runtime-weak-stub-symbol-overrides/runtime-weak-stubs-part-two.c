// SPDX-License-Identifier: Apache-2.0
// runtime-weak-stubs-part-two.c — weak stubs (fail-CLOSED on enforcement path)
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#pragma weak match_name_against_exemption_table
const char *match_name_against_exemption_table(const char *name) {
    (void)name;
    fprintf(stderr, "FATAL: match_name_against_exemption_table stub called — not implemented\n");
    exit(1);
}

#pragma weak check_non_source_file_allowlist
int check_non_source_file_allowlist(const char *name) {
    (void)name;
    fprintf(stderr, "FATAL: check_non_source_file_allowlist stub called — not implemented\n");
    exit(1);
}

#pragma weak match_name_against_bootstrap_list
int match_name_against_bootstrap_list(const char *basename) {
    (void)basename;
    fprintf(stderr, "FATAL: match_name_against_bootstrap_list stub called — not implemented\n");
    exit(1);
}

#pragma weak check_single_file_for_violations
void check_single_file_for_violations(const char *sub, int is_c, int is_source,
    void *fns, int *fn_qty, void *incs, int *inc_qty)
{
    (void)sub; (void)is_c; (void)is_source;
    (void)fns; (void)fn_qty;
    (void)incs; (void)inc_qty;
    fprintf(stderr, "FATAL: check_single_file_for_violations stub called — not implemented\n");
    exit(1);
}

#pragma weak search_for_unused_function_code
void search_for_unused_function_code(void *fns, int fn_qty, const char *srcdir)
{
    (void)fns; (void)fn_qty; (void)srcdir;
    fprintf(stderr, "FATAL: search_for_unused_function_code stub called — not implemented\n");
    exit(1);
}

#pragma weak check_name_against_allowed_whitelist
int check_name_against_allowed_whitelist(const char *name) {
    (void)name;
    fprintf(stderr, "FATAL: check_name_against_allowed_whitelist stub called — not implemented\n");
    exit(1);
}
