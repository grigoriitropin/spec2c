// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin

#pragma once
#include <stddef.h>
#include <cjson/cJSON.h>

// --- scaffold linkage ---

// Declared by generated scaffold; callable from core.
_Noreturn void usage(const char *prog);

// --- error output ---

// Print JSON error to stdout, message to stderr, exit(1).
_Noreturn void vl_die(const char *tool, const char *error);

// --- config (broker KEY=VALUE files) ---

// Resolve default config path: $HOME/.config/vehir/env
// Returns malloc'd string or NULL (caller frees).
char *vl_default_config_path(void);

// Read a single KEY=VALUE from path. Returns malloc'd value or NULL.
// Refuses group/other-readable files (prints warning, returns NULL).
char *vl_cfg_load(const char *tool, const char *path, const char *key);

// Like vl_cfg_load but calls vl_die on failure.
char *vl_cfg_require(const char *tool, const char *path, const char *key);

// --- DB types (opaque; real definitions in db.h) ---

#ifndef VEHIR_DB_H
typedef struct vehir_db vehir_db;
typedef struct vehir_db_result vehir_db_result;
#endif

// --- DB helpers ---

// Call vl_die if db handle is null or has error.
void vl_db_check(const char *tool, vehir_db *db);

// Run a parameterised query, return result as cJSON {nrows, ncols, columns, rows}.
// Calls vl_die on error.
cJSON *vl_db_query_json(const char *tool, vehir_db *db,
                         const char *sql,
                         const char *const *params, int nparams);

// --- safe exec ---

// fork+exec argv[0] with full argv. Returns exit code or -1 on fork failure.
// Never passes through a shell.
int vl_safe_exec(char *const argv[]);
