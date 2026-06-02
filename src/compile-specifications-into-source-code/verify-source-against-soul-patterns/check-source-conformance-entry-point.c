// SPDX-License-Identifier: Apache-2.0
#include "share-check-types-and-declarations.h"

const char *SPEC2C_CHECK_VERSION = "0.3";

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
        if (len + 1 >= cap) {
                cap *= 2;
                char *t = realloc(buf, cap);
                if (!t) { free(buf); terminate_with_error_message_output("realloc"); }
                buf = t;
            }
    }
    if (ferror(f)) { free(buf); terminate_with_error_message_output("read error"); }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

void initialize_deviation_list_data_structure(deviation_list_t *dl) {
    dl->cap = 32;
    dl->items = malloc((size_t)dl->cap * sizeof(deviation_t));
    if (!dl->items) terminate_with_error_message_output("malloc");
    dl->count = 0;
}

void append_deviation_into_report_list(deviation_list_t *dl, const char *check_id, const char *pattern_id,
                   severity_t sev, int line, int col, const char *source_line,
                   const char *msg, const char *sug, int in_scaffold) {
    if (dl->count >= dl->cap) {
        dl->cap *= 2;
        deviation_t *t = realloc(dl->items, (size_t)dl->cap * sizeof(deviation_t));
        if (!t) terminate_with_error_message_output("realloc");
        dl->items = t;
    }
    deviation_t *d = &dl->items[dl->count++];
    d->check_id = check_id;
    d->pattern_id = pattern_id;
    d->severity = sev;
    d->line = line;
    d->col = col;
    d->source_line = source_line ? strdup(source_line) : strdup("");
    d->message = msg;
    d->suggestion = sug;
    d->in_scaffold = in_scaffold;
}

    return 0;
}

    return NULL;
}

char *execute_ast_grep_with_pattern(const char *sg_cmd, const char *file_path, const char *pattern) {
    char *args[] = {(char*)sg_cmd, (char*)"run", (char*)"-l", (char*)"c",
                    (char*)"-p", (char*)pattern, (char*)"--json", (char*)file_path, NULL};

    pid_t pid;
    FILE *p = execute_command_capture_stdout_pipe(args, &pid);
    if (!p) return NULL;

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) { close_pipe_await_child_finish(p, pid); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += n;
        if (len + 1 >= cap) {
                    cap *= 2;
                    char *t = realloc(buf, cap);
                    if (!t) { free(buf); close_pipe_await_child_finish(p, pid); return NULL; }
                    buf = t;
                }
    }
    buf[len] = '\0';
    close_pipe_await_child_finish(p, pid);
    return buf;
}

void write_conformance_report_into_output(const deviation_list_t *dl, const char *file_path,
                         int scaffold_ok, int error_count, int warning_count) {
    cJSON *root = cJSON_CreateObject();
    if (!root) terminate_with_error_message_output("cJSON alloc failed");

    int ok = (error_count == 0);
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddStringToObject(root, "file", file_path);
    cJSON_AddNumberToObject(root, "total_deviations", dl->count);
    cJSON_AddNumberToObject(root, "errors", error_count);
    cJSON_AddNumberToObject(root, "warnings", warning_count);
    cJSON_AddBoolToObject(root, "scaffold_ok", scaffold_ok);

    cJSON *deviations = cJSON_AddArrayToObject(root, "deviations");
    for (int i = 0; i < dl->count; i++) {
        deviation_t *d = &dl->items[i];
        cJSON *dev = cJSON_CreateObject();
        cJSON_AddStringToObject(dev, "check_id", d->check_id);
        cJSON_AddStringToObject(dev, "pattern_id", d->pattern_id);
        cJSON_AddStringToObject(dev, "severity", convert_severity_into_readable_string(d->severity));
        cJSON_AddNumberToObject(dev, "line", d->line);
        cJSON_AddNumberToObject(dev, "column", d->col);
        if (d->source_line && d->source_line[0])
            cJSON_AddStringToObject(dev, "source", d->source_line);
        cJSON_AddStringToObject(dev, "message", d->message ? d->message : "");
        cJSON_AddStringToObject(dev, "suggestion", d->suggestion ? d->suggestion : "");
        if (d->in_scaffold >= 0)
            cJSON_AddBoolToObject(dev, "in_scaffold", d->in_scaffold);
        cJSON_AddItemToArray(deviations, dev);
    }

    char *js = cJSON_Print(root);
    if (js) { printf("%s\n", js); free(js); }
    cJSON_Delete(root);
}


    }

    if (!file_path) terminate_with_error_message_output("C source file required");
    if (!base_dir) base_dir = ".";
    if (!patterns_path) {
        static char default_patterns[PATH_MAX_SZ];
        snprintf(default_patterns, sizeof(default_patterns), "%s/soul-patterns.json", base_dir);
        patterns_path = default_patterns;
    }

    if (!verify_ast_grep_command_exists())
        terminate_with_error_message_output("ast-grep not found in PATH — required for C AST parsing");

    const char *sg_cmd = find_ast_grep_executable_path();

    char *file_content = read_entire_text_into_memory(file_path);

    deviation_list_t dl;
    initialize_deviation_list_data_structure(&dl);

    pattern_def_t *patterns = NULL;
    int npatterns = 0;
    if (!read_pattern_definitions_from_json(patterns_path, &patterns, &npatterns))
        terminate_with_error_message_output("cannot load soul-patterns.json");

    for (int i = 0; i < npatterns; i++) {
        pattern_def_t *pat = &patterns[i];

        for (int j = 0; j < pat->nidentifiers; j++)
            match_identifier_against_ast_grep(pat, pat->identifiers[j], sg_cmd, file_path, file_content, &dl);

        if (pat->nforbidden_strings > 0)
            match_string_against_file_content(pat, file_path, file_content, &dl);
    }

    scan_for_scaffold_marker_deviations(file_content, spec_path ? spec_path : file_path, &dl);

    int error_count = 0, warning_count = 0;
    for (int i = 0; i < dl.count; i++) {
        if (dl.items[i].severity == SEV_ERROR) error_count++;
        else if (dl.items[i].severity == SEV_WARNING) warning_count++;
    }

    int scaffold_ok = 1;
    for (int i = 0; i < dl.count; i++) {
        if (strcmp(dl.items[i].check_id, "scaffold-marker") == 0 &&
            dl.items[i].severity == SEV_ERROR) {
            scaffold_ok = 0;
            break;
        }
    }

    write_conformance_report_into_output(&dl, file_path, scaffold_ok, error_count, warning_count);

    free(file_content);
    for (int i = 0; i < dl.count; i++) free((void *)dl.items[i].source_line);
    free(dl.items);

    for (int i = 0; i < npatterns; i++) {
        free((void *)patterns[i].id);
        free((void *)patterns[i].severity_str);
        free((void *)patterns[i].message);
        free((void *)patterns[i].suggestion);
        for (int j = 0; j < patterns[i].nidentifiers; j++)
            free((void *)patterns[i].identifiers[j]);
        free(patterns[i].identifiers);
        for (int j = 0; j < patterns[i].nforbidden_strings; j++)
            free((void *)patterns[i].forbidden_strings[j]);
        free(patterns[i].forbidden_strings);
        for (int j = 0; j < patterns[i].nsql_patterns; j++)
            free((void *)patterns[i].sql_patterns[j]);
        free(patterns[i].sql_patterns);
    }
    free(patterns);

    return error_count > 0 ? 1 : 0;
}

int main(int argc, char *argv[])") != NULL);
    int has_help = (strstr(file_content, "--help") != NULL);
    int has_vl_die = (strstr(file_content, "vehir_fatal_error_and_exit(") != NULL);
    int has_system = (strstr(file_content, "system(") != NULL);
    int has_popen = (strstr(file_content, "popen(") != NULL);

    if (!has_vehir_lib)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-include", SEV_ERROR, 0, 0, "",
               "Missing #include \"vehir-shared-abstraction-wrapper-code.h\" — scaffold not present",
               "Add #include \"vehir-shared-abstraction-wrapper-code.h\"", -1);

    if (!has_usage)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-usage", SEV_ERROR, 0, 0, "",
               "Missing _Noreturn usage() function",
               "Add usage() function generated by spec2c", -1);

    if (!has_extern)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-extern", SEV_INFO, 0, 0, "",
               "No extern core function declaration found",
               "Add extern int <core_fn>(...) declaration", -1);

    if (!has_main)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-main", SEV_ERROR, 0, 0, "",
               "Missing int main(int argc, char *argv[])",
               "Add main() scaffold from spec2c", -1);

    if (!has_help)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-help", SEV_WARNING, 0, 0, "",
               "No --help handling found",
               "Add --help/-h check calling usage()", -1);

    if (has_system)
        append_deviation_into_report_list(dl, "scaffold-marker", "scaffold-system", SEV_ERROR, 0, 0, "",
               "system() call in scaffold — must use execute_command_with_argument_list()",
               "Replace system() with execute_command_with_argument_list()", -1);

    if (has_popen)
        append_deviation_into_report_list(dl, "scaffold-marker", "scaffold-popen", SEV_ERROR, 0, 0, "",
               "popen() call in scaffold — must use execute_command_with_argument_list()",
               "Replace popen() with execute_command_with_argument_list()", -1);

    if (!has_vl_die)
        append_deviation_into_report_list(dl, "scaffold-marker", "missing-vl-die", SEV_WARNING, 0, 0, "",
               "No vehir_fatal_error_and_exit() usage — scaffold should use vehir_fatal_error_and_exit() for structured errors",
               "Replace bare exit()/fprintf+exit with vehir_fatal_error_and_exit()", -1);
}
