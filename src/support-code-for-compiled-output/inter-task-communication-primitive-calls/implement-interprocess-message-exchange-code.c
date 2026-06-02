// SPDX-License-Identifier: Apache-2.0
// ipm_builtins_ipc.c — IPC for .ipm: call C tools from .ipm via fork/exec/pipe
// Optional runtime module. Not core.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <cjson/cJSON.h>

static int ipc_depth = 0;
#define IPC_MAX_DEPTH 4
#define IPC_TIMEOUT   10

cJSON* call_ipm_message_exchange_code(const char *tool, const char *arg) {
    if (!tool || ipc_depth >= IPC_MAX_DEPTH) return NULL;

    int pipefd[2];
    if (pipe(pipefd) != 0) return NULL;

    ipc_depth++;
    pid_t pid = fork();

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp(tool, tool, arg, NULL);
        _exit(127);
    }

    close(pipefd[1]);

    /* wait with timeout */
    struct timeval tv = {IPC_TIMEOUT, 0};
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);

    int ready = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    cJSON *result = NULL;

    if (ready > 0) {
        char buf[65536] = {0};
        ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
        if (n > 0) result = cJSON_Parse(buf);
    } else {
        kill(pid, SIGKILL);
    }

    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    ipc_depth--;
    return result;
}
