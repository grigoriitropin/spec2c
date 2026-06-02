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
#include "ipm_builtins.h"
#include "enforce_sub/enforce.h"

#define SUBST_MAX 32
#define VAL_SZ   4096

typedef struct { const char *key; char val[VAL_SZ]; } subst_t;
typedef struct {
    const char *name, *ident, *description, *core_function;
    int nconfig_keys, has_commands, has_db;
    char *config_keys_str;
} spec_t;
typedef struct { cJSON *meta; const char *name, *type, *desc; } ipm_spec_t;

const char *jstr(const cJSON *obj, const char *key);
_Noreturn void die(const char *msg);
char *name_to_ident(const char *name);
char *read_file(const char *path);
void ipm_add_subst(subst_t *subs, int *n, const char *k, const char *v);
void compile_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type);
void compile_functions_to_c(const ipm_spec_t *spec, FILE *out, int is_library);
void generate_header(const ipm_spec_t *spec, const char *hdr_path);
void generate_from_ipm(const ipm_spec_t *spec, const char *out_path, int is_library);
void parse_spec_from_cjson(cJSON *root, spec_t *s);
void compute_substs(const spec_t *s, subst_t *subs, int *nsubs);
char *subst_apply(const char *tmpl, const subst_t *subs, int nsubs);
char *resolve_template(const char *base, const char *file);

#endif
