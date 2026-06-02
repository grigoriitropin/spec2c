// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_ipc.h — IPC builtins (optional runtime module)
#ifndef IPM_BUILTINS_IPC_H
#define IPM_BUILTINS_IPC_H

#include <cjson/cJSON.h>

cJSON* ipm_call_c(const char *tool, const char *arg);

#endif
