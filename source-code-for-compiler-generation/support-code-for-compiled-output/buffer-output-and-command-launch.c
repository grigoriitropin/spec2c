// SPDX-License-Identifier: Apache-2.0
// ipm_builtins — buffer output, arg access, exec wrappers (split part 3)
#include "../runtime-for-generated-ipm-code.h"
#include <unistd.h>
#include <sys/wait.h>

void write_string_buffer_into_file(string_buffer *buf, const char *path) {
    if (!buf || !path) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
}

void flush_string_buffer_into_stdout(string_buffer *buf) {
    if (!buf) return;
    fwrite(buf->data, 1, buf->len, stdout);
    fflush(stdout);
}

void free_allocated_string_buffer_memory(string_buffer *buf) {
    if (!buf) return;
    free(buf->data);
    free(buf);
}

int g_argc = 0;
char **g_argv = NULL;

int get_argument_count_from_global(void) { return g_argc; }

char *get_argument_value_from_global(int index) {
    if (index >= 0 && index < g_argc && g_argv)
        return g_argv[index];
    return "";
}

int launch_command_with_argument_array(char *const argv[]) {
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
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return NULL;
    }
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
