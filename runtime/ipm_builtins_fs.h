// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_fs.h — filesystem wrappers (optional module)
// Ownership: directory_list() returns heap cJSON* — caller must cJSON_Delete()
#ifndef IPM_BUILTINS_FS_H
#define IPM_BUILTINS_FS_H

#include <cjson/cJSON.h>

cJSON* directory_list(const char *path);
int    file_exists(const char *path);
int    dir_exists(const char *path);

#endif
