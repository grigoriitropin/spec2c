// SPDX-License-Identifier: Apache-2.0
#include "core.h"

const char *SPEC2C_CHECK_VERSION = "0.3";

int safe_exec(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) { execvp(argv[0], argv); _exit(127); }
    int status; waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

FILE *safe_popen_read(char *const argv[], pid_t *out_pid) {
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

int safe_pclose(FILE *fp, pid_t pid) {
    fclose(fp);
    int status; waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#include <errno.h>
#include <cjson/cJSON.h>

#define MAX_LINE    4096
#define SG_TIMEOUT  30
#define PATH_MAX_SZ 1024

typedef enum { SEV_INFO, SEV_WARNING, SEV_ERROR } severity_t;

typedef struct {
    const char *check_id;
    const char *pattern_id;
    severity_t   severity;
    int          line;
    int          col;
    const char *source_line;
    const char *message;
    const char *suggestion;
    int          in_scaffold;
} deviation_t;

typedef struct {
    deviation_t *items;
    int          count;
    int          cap;
} deviation_list_t;

typedef struct {
    const char *id;
    const char **identifiers;
    int          nidentifiers;
    const char **forbidden_strings;
    int          nforbidden_strings;
    const char **sql_patterns;
    int          nsql_patterns;
    int          null_check_window;
    const char *message;
    const char *suggestion;
    const char *severity_str;
    severity_t   severity;
} pattern_def_t;

_Noreturn void die(const char *msg) {
    fprintf(stderr, "spec2c-check: %s\n", msg);
    printf("{\"ok\":false,\"error\":\"%s\"}\n", msg);
    exit(1);
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        char buf[512];
        snprintf(buf, sizeof(buf), "cannot open %s: %s", path, strerror(errno));
        die(buf);
    }
    size_t cap = 16384, len = 0;
    char *buf = malloc(cap);
    if (!buf) die("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); die("realloc"); } buf = t; }
    }
    if (ferror(f)) { free(buf); die("read error"); }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

void dl_init(deviation_list_t *dl) {
    dl->cap = 32;
    dl->items = malloc((size_t)dl->cap * sizeof(deviation_t));
    if (!dl->items) die("malloc");
    dl->count = 0;
}

void dl_add(deviation_list_t *dl, const char *check_id, const char *pattern_id,
                   severity_t sev, int line, int col, const char *source_line,
                   const char *msg, const char *sug, int in_scaffold) {
    if (dl->count >= dl->cap) {
        dl->cap *= 2;
        deviation_t *t = realloc(dl->items, (size_t)dl->cap * sizeof(deviation_t));
        if (!t) die("realloc");
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

const char *sev_str(severity_t s) {
    switch (s) {
        case SEV_INFO:    return "info";
        case SEV_WARNING: return "warning";
        case SEV_ERROR:   return "error";
        default:          return "unknown";
    }
}

severity_t parse_severity(const char *s) {
    if (strcmp(s, "error") == 0) return SEV_ERROR;
    if (strcmp(s, "warning") == 0) return SEV_WARNING;
    return SEV_INFO;
}

char *shell_escape(const char *s) {
    size_t len = strlen(s);
    char *out = malloc(len * 2 + 3);
    if (!out) die("malloc");
    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '"') { out[pos++] = '\\'; out[pos++] = '"'; }
        else if (s[i] == '\\') { out[pos++] = '\\'; out[pos++] = '\\'; }
        else out[pos++] = s[i];
    }
    out[pos] = '\0';
    return out;
}

int sg_available(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char *args[] = {(char*)candidates[i], (char*)"--version", NULL};
        if (safe_exec(args) == 0) return 1;
    }
    return 0;
}

const char *sg_path(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char *args[] = {(char*)candidates[i], (char*)"--version", NULL};
        if (safe_exec(args) == 0) return candidates[i];
    }
    return NULL;
}

char *run_ast_grep(const char *sg_cmd, const char *file_path, const char *pattern) {
    char *args[] = {(char*)sg_cmd, (char*)"run", (char*)"-l", (char*)"c",
                    (char*)"-p", (char*)pattern, (char*)"--json", (char*)file_path, NULL};

    pid_t pid;
    FILE *p = safe_popen_read(args, &pid);
    if (!p) return NULL;

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) { safe_pclose(p, pid); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); safe_pclose(p, pid); return NULL; } buf = t; }
    }
    buf[len] = '\0';
    safe_pclose(p, pid);
    return buf;
}

void emit_report(const deviation_list_t *dl, const char *file_path,
                         int scaffold_ok, int error_count, int warning_count) {
    cJSON *root = cJSON_CreateObject();
    if (!root) die("cJSON alloc failed");

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
        cJSON_AddStringToObject(dev, "severity", sev_str(d->severity));
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
            if (++i >= argc) die("missing argument for --spec");
            spec_path = argv[i];
        } else if (strcmp(argv[i], "--base") == 0) {
            if (++i >= argc) die("missing argument for --base");
            base_dir = argv[i];
        } else if (strcmp(argv[i], "--patterns") == 0) {
            if (++i >= argc) die("missing argument for --patterns");
            patterns_path = argv[i];
        } else if (!file_path) {
            file_path = argv[i];
        } else {
            die("unexpected argument");
        }
    }

    if (!file_path) die("C source file required");
    if (!base_dir) base_dir = ".";
    if (!patterns_path) {
        static char default_patterns[PATH_MAX_SZ];
        snprintf(default_patterns, sizeof(default_patterns), "%s/soul-patterns.json", base_dir);
        patterns_path = default_patterns;
    }

    if (!sg_available())
        die("ast-grep not found in PATH — required for C AST parsing");

    const char *sg_cmd = sg_path();

    char *file_content = read_file(file_path);

    deviation_list_t dl;
    dl_init(&dl);

    pattern_def_t *patterns = NULL;
    int npatterns = 0;
    if (!load_patterns(patterns_path, &patterns, &npatterns))
        die("cannot load soul-patterns.json");

    for (int i = 0; i < npatterns; i++) {
        pattern_def_t *pat = &patterns[i];

        for (int j = 0; j < pat->nidentifiers; j++)
            check_identifier_pattern(pat, pat->identifiers[j], sg_cmd, file_path, file_content, &dl);

        if (pat->nforbidden_strings > 0)
            check_string_pattern(pat, file_path, file_content, &dl);
    }

    check_scaffold_markers(file_content, spec_path ? spec_path : file_path, &dl);

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

    emit_report(&dl, file_path, scaffold_ok, error_count, warning_count);

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
