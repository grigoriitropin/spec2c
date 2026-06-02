// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_ipc.h — IPC builtins (optional runtime module)
#ifndef IPM_BUILTINS_IPC_H
#define IPM_BUILTINS_IPC_H

#include <cjson/cJSON.h>

/* Poison raw exec — force ipm_call_c() or pipeline composition */
#ifdef IPM_POISON_RAW_EXEC
#define popen(...)    _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of popen()\"")
#define system(...)   _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of system()\"")
#define execlp(...)   _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of execlp()\"")
#define execvp(...)   _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of execvp()\"")
#define execve(...)   _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of execve()\"")
#define posix_spawn(...) _Pragma("GCC error \"Use ipm_call_c() or pipeline composition instead of posix_spawn()\"")
#endif


cJSON* ipm_call_c(const char *tool, const char *arg);

#endif
