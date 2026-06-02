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
_Noreturn void vehir_fatal_error_and_exit(const char *tool, const char *error);

// --- config (broker KEY=VALUE files) ---

// Resolve default config path: $HOME/.config/vehir/env
// Returns malloc'd string or NULL (caller frees).
char *resolve_default_config_file_path(void);

// Read a single KEY=VALUE from path. Returns malloc'd value or NULL.
// Refuses group/other-readable files (prints warning, returns NULL).
char *load_vehir_configuration_from_file(const char *tool, const char *path, const char *key);

// Like load_vehir_configuration_from_file but calls vehir_fatal_error_and_exit on failure.
char *require_config_value_else_die(const char *tool, const char *path, const char *key);

// --- DB types (opaque; real definitions in db.h) ---

#ifndef VEHIR_DB_H
typedef struct vehir_db vehir_db;
typedef struct vehir_db_result vehir_db_result;
#endif

// --- DB helpers ---

// Call vehir_fatal_error_and_exit if db handle is null or has error.
void verify_database_connection_still_alive(const char *tool, vehir_db *db);

// Run a parameterised query, return result as cJSON {nrows, ncols, columns, rows}.
// Calls vehir_fatal_error_and_exit on error.
cJSON *query_database_and_return_json(const char *tool, vehir_db *db,
                         const char *sql,
                         const char *const *params, int nparams);

// --- safe exec ---

// fork+exec argv[0] with full argv. Returns exit code or -1 on fork failure.
// Never passes through a shell.
int execute_command_with_argument_list(char *const argv[]);
