// SPDX-License-Identifier: Apache-2.0
#include "share-check-types-and-declarations.h"

int launch_command_with_argument_list(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) { execvp(argv[0], argv); _exit(127); }
    int status; waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

FILE *execute_command_capture_stdout_pipe(char *const argv[], pid_t *out_pid) {
    int pfd[2];
    if (pipe(pfd) < 0) return NULL;
    pid_t pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return NULL; }
    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pfd[1]);
    if (out_pid) *out_pid = pid;
    return fdopen(pfd[0], "r");
}

int close_pipe_await_child_finish(FILE *fp, pid_t pid) {
    fclose(fp);
    int status; waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

_Noreturn void terminate_with_error_message_output(const char *msg) {
    fprintf(stderr, "spec2c-check: %s\n", msg);
    printf("{\"ok\":false,\"error\":\"%s\"}\n", msg);
    exit(1);
}

char *read_entire_text_into_memory(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        char buf[512];
        snprintf(buf, sizeof(buf), "cannot open %s: %s", path, strerror(errno));
        terminate_with_error_message_output(buf);
    }
    size_t cap = 16384, len = 0;
    char *buf = malloc(cap);
    if (!buf) terminate_with_error_message_output("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); terminate_with_error_message_output("realloc"); } buf = t; }
    }
    if (ferror(f)) { free(buf); terminate_with_error_message_output("read error"); }
    buf[len] = '\0';
    fclose(f);
    return buf;
}
