// SPDX-License-Identifier: Apache-2.0
#include "share-check-types-and-declarations.h"

const char *SPEC2C_CHECK_VERSION = "0.3";

void initialize_deviation_list_structure(deviation_list_t *dl) {
    dl->cap = 32;
    dl->items = malloc((size_t)dl->cap * sizeof(deviation_t));
    if (!dl->items) terminate_with_error_message_output("malloc");
    dl->count = 0;
}

void append_deviation_to_report_list(deviation_list_t *dl, const char *check_id, const char *pattern_id,
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

const char *convert_severity_into_readable_string(severity_t s) {
    switch (s) {
        case SEV_INFO:    return "info";
        case SEV_WARNING: return "warning";
        case SEV_ERROR:   return "error";
        default:          return "unknown";
    }
}

severity_t parse_severity_from_config_string(const char *s) {
    if (strcmp(s, "error") == 0) return SEV_ERROR;
    if (strcmp(s, "warning") == 0) return SEV_WARNING;
    return SEV_INFO;
}

char *escape_string_for_shell_command(const char *s) {
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 3);
    if (!out) terminate_with_error_message_output("malloc");
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '"') { out[pos++] = '\\'; out[pos++] = '"'; }
        else if (s[i] == '\\') { out[pos++] = '\\'; out[pos++] = '\\'; }
        else out[pos++] = s[i];
    }
    out[pos] = '\0';
    return out;
}

int verify_ast_grep_command_exists(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char *args[] = {(char*)candidates[i], (char*)"--version", NULL};
        if (launch_command_with_argument_list(args) == 0) return 1;
    }
    return 0;
}

const char *find_ast_grep_executable_path(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char *args[] = {(char*)candidates[i], (char*)"--version", NULL};
        if (launch_command_with_argument_list(args) == 0) return candidates[i];
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
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); close_pipe_await_child_finish(p, pid); return NULL; } buf = t; }
    }
    buf[len] = '\0';
    close_pipe_await_child_finish(p, pid);
    return buf;
}

void write_conformance_report_to_output(const deviation_list_t *dl, const char *file_path,
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

int main(int argc, char *argv[]) {
    const char *file_path = NULL;
    const char *spec_path = NULL;
    const char *base_dir = NULL;
    const char *patterns_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: spec2c-check [options] <file.c>\n"
                "\n"
                "  <file.c>              C source file to check\n"
                "  --spec <spec.json>    Spec file for scaffold comparison\n"
                "  --base <dir>          Template/skeleton base directory\n"
                "  --patterns <file>     Custom patterns file (default: soul-patterns.json)\n"
                "  --help, -h\n"
                "\n"
                "Checks scaffold conformance and SOUL compliance.\n"
                "Emits JSON report to stdout.\n");
            return 0;
        } else if (strcmp(argv[i], "--spec") == 0) {
            if (++i >= argc) terminate_with_error_message_output("missing argument for --spec");
            spec_path = argv[i];
        } else if (strcmp(argv[i], "--base") == 0) {
            if (++i >= argc) terminate_with_error_message_output("missing argument for --base");
            base_dir = argv[i];
        } else if (strcmp(argv[i], "--patterns") == 0) {
            if (++i >= argc) terminate_with_error_message_output("missing argument for --patterns");
            patterns_path = argv[i];
        } else if (!file_path) {
            file_path = argv[i];
        } else {
            terminate_with_error_message_output("unexpected argument");
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
    initialize_deviation_list_structure(&dl);

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

    write_conformance_report_to_output(&dl, file_path, scaffold_ok, error_count, warning_count);

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
