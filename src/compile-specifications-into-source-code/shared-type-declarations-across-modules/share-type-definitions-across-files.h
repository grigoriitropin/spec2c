// SPDX-License-Identifier: Apache-2.0
// common.h — shared types and declarations for spec2c compiler

#ifndef SPEC2C_COMMON_H
#define SPEC2C_COMMON_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cjson/cJSON.h>
#include <dirent.h>
#include <sys/stat.h>
#include "runtime-for-generated-ipm-code.h"

#define SUBST_MAX 32
#define VAL_SZ   4096

typedef struct {
    const char *key;
    char val[VAL_SZ];
} subst_t;
typedef struct {
    const char *name, *ident, *description, *core_function;
    int nconfig_keys, has_commands, has_db;
    char *config_keys_str;
} spec_t;
typedef struct {
    cJSON *meta;
    const char *name, *type, *desc;
} ipm_spec_t;

const char *extract_json_field_string_value(const cJSON *obj, const char *key);
_Noreturn void report_fatal_error_and_exit(const char *msg);
char *convert_hyphen_name_into_underscore(const char *name);
char *read_entire_file_into_memory(const char *path);
void append_key_value_into_substitution(subst_t *subs, int *n, const char *k, const char *v);
void generate_code_from_ast_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type);
void compile_every_function_into_code(const ipm_spec_t *spec, FILE *out, int is_library);
void write_component_header_with_prototypes(const ipm_spec_t *spec, const char *hdr_path);
void handle_ipm_spec_emit_code(const ipm_spec_t *spec, const char *out_path, int is_library);
void parse_legacy_object_format_json(cJSON *root, spec_t *s);
void create_substitution_table_for_spec(const spec_t *s, subst_t *subs, int *nsubs);
char *apply_substitution_against_text_data(const char *tmpl, const subst_t *subs, int nsubs);
char *resolve_template_file_from_base(const char *base, const char *file);

/* ── SOUL §10 banned type words (canonical list — keep in sync with enforce-naming-whitelist-and-validation.c) ── */
static const char *soul_banned_type_words[] = {
    "service","server","daemon","library","tool","binary",
    "package","module","system","utility","application",
    "program","process","worker",NULL
};

#endif
