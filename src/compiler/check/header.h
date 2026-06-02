// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin
//
// spec2c-check v0.3 — conformance checker for spec2c-generated C tools
//
// Reverse direction of spec2c: parses C (AST via ast-grep) → checks against
// the same canonical skeleton spec2c generates from → emits JSON report of
// scaffold deviations (shell-out, output suppression, SQL interpolation,
// unchecked returns) with file:line + fix suggestions.
//
// Architectural key: ONE definition of the canonical pattern (soul-patterns.json),
// shared by spec2c (generate) and this checker (check). Generate → check = clean
// is the built-in self-test.
//
// Modes:
//   spec2c-check <file.c>                         → pattern-only scan
//   spec2c-check <file.c> --spec <spec.json>       → full check with scaffold compare
//   spec2c-check <file.c> --spec <spec.json> --base <dir>  → custom template base

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ── safe exec wrappers (no system/popen, no /dev/null) ───────────── */
#include <errno.h>
#include <cjson/cJSON.h>

#define MAX_LINE    4096
#define SG_TIMEOUT  30
#define PATH_MAX_SZ 1024

typedef enum { SEV_INFO, SEV_WARNING, SEV_ERROR } severity_t;

typedef struct {
    const char *check_id;
    const char *pattern_id;
    severity_t   severity;
    int          line;
    int          col;
    const char *source_line;
    const char *message;
    const char *suggestion;
    int          in_scaffold;
} deviation_t;

typedef struct {
    deviation_t *items;
    int          count;
    int          cap;
} deviation_list_t;

typedef struct {
    const char *id;
    const char **identifiers;
    int          nidentifiers;
    const char **forbidden_strings;
    int          nforbidden_strings;
    const char **sql_patterns;
    int          nsql_patterns;
    int          null_check_window;
    const char *message;
    const char *suggestion;
    const char *severity_str;
    severity_t   severity;
} pattern_def_t;


int safe_exec(char *const argv[]);
FILE *safe_popen_read(char *const argv[], pid_t *out_pid);
int safe_pclose(FILE *fp, pid_t pid);
_Noreturn void die(const char *msg);
char *read_file(const char *path);
void dl_init(deviation_list_t *dl);
void dl_add(deviation_list_t *dl, const char *check_id, const char *pattern_id,
            severity_t sev, int line, int column, const char *identifier,
            const char *message, const char *fix, int skeleton_line);
const char *sev_str(severity_t s);
severity_t parse_severity(const char *s);
char *shell_escape(const char *s);
int sg_available(void);
const char *sg_path(void);
char *run_ast_grep(const char *sg_cmd, const char *file_path, const char *pattern);
void check_identifier_pattern(const pattern_def_t *pat, const char *identifier,
                              const char *sg_cmd, const char *file_path,
                              const char *file_content, deviation_list_t *dl);
void check_string_pattern(const pattern_def_t *pat, const char *file_path,
                          const char *file_content, deviation_list_t *dl);
void check_scaffold_markers(const char *file_content, const char *spec_name,
                            deviation_list_t *dl);
int load_patterns(const char *path, pattern_def_t **patterns_out, int *npatterns_out);
void emit_report(const deviation_list_t *dl, const char *file_path,
                 int scaffold_ok, int error_count, int warning_count);
