// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_fs.h — filesystem wrappers (optional module)
// Ownership: directory_list() returns heap cJSON* — caller must cJSON_Delete()
#ifndef IPM_BUILTINS_FS_H
#define IPM_BUILTINS_FS_H

#include <cjson/cJSON.h>

cJSON* directory_list(const char *path);
int    file_exists(const char *path);
int    dir_exists(const char *path);
int    string_count_newlines(const char *text);
char*  file_read_bytes(const char *path);
void   file_write_bytes(const char *path, const char *data);
int    file_rename(const char *old_path, const char *new_path);
int    file_unlink(const char *path);

#endif
