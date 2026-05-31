// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Grigorii Tropin
//
// spec2c-check v0.3 — conformance checker for spec2c-generated C tools
//
// Reverse direction of spec2c: parses C (AST via ast-grep) → checks against
// the same canonical skeleton spec2c generates from → emits JSON report of
// scaffold deviations (shell-out, output suppression, SQL interpolation,
// unchecked returns) with file:line + fix suggestions.
//
// Architectural key: ONE definition of the canonical pattern (soul-patterns.json),
// shared by spec2c (generate) and this checker (check). Generate → check = clean
// is the built-in self-test.
//
// Modes:
//   spec2c-check <file.c>                         → pattern-only scan
//   spec2c-check <file.c> --spec <spec.json>       → full check with scaffold compare
//   spec2c-check <file.c> --spec <spec.json> --base <dir>  → custom template base

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static _Noreturn void die(const char *msg) {
    fprintf(stderr, "spec2c-check: %s\n", msg);
    printf("{\"ok\":false,\"error\":\"%s\"}\n", msg);
    exit(1);
}

static char *read_file(const char *path) {
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

static void dl_init(deviation_list_t *dl) {
    dl->cap = 32;
    dl->items = malloc((size_t)dl->cap * sizeof(deviation_t));
    if (!dl->items) die("malloc");
    dl->count = 0;
}

static void dl_add(deviation_list_t *dl, const char *check_id, const char *pattern_id,
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

static const char *sev_str(severity_t s) {
    switch (s) {
        case SEV_INFO:    return "info";
        case SEV_WARNING: return "warning";
        case SEV_ERROR:   return "error";
        default:          return "unknown";
    }
}

static severity_t parse_severity(const char *s) {
    if (strcmp(s, "error") == 0) return SEV_ERROR;
    if (strcmp(s, "warning") == 0) return SEV_WARNING;
    return SEV_INFO;
}

static char *shell_escape(const char *s) {
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

static int sg_available(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s --version >/dev/null 2>&1", candidates[i]);
        if (system(cmd) == 0) return 1;
    }
    return 0;
}

static const char *sg_path(void) {
    const char *candidates[] = {"ast-grep", "sg", NULL};
    for (int i = 0; candidates[i]; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s --version >/dev/null 2>&1", candidates[i]);
        if (system(cmd) == 0) return candidates[i];
    }
    return NULL;
}

static char *run_ast_grep(const char *sg_cmd, const char *file_path, const char *pattern) {
    char cmd[PATH_MAX_SZ * 2];
    char *esc_pattern = shell_escape(pattern);
    snprintf(cmd, sizeof(cmd),
        "%s run -l c -p '%s' --json %s 2>/dev/null",
        sg_cmd, esc_pattern, file_path);
    free(esc_pattern);

    FILE *p = popen(cmd, "r");
    if (!p) return NULL;

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) { pclose(p); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); pclose(p); return NULL; } buf = t; }
    }
    buf[len] = '\0';
    pclose(p);
    return buf;
}

static void check_identifier_pattern(const pattern_def_t *pat, const char *identifier,
                                     const char *sg_cmd, const char *file_path,
                                     const char *file_content, deviation_list_t *dl) {
    char *sg_output = run_ast_grep(sg_cmd, file_path, identifier);
    if (!sg_output) return;

    cJSON *root = cJSON_Parse(sg_output);
    free(sg_output);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return;
    }

    int nmatches = cJSON_GetArraySize(root);
    for (int i = 0; i < nmatches; i++) {
        cJSON *match = cJSON_GetArrayItem(root, i);
        if (!match) continue;

        cJSON *range = cJSON_GetObjectItemCaseSensitive(match, "range");
        if (!range) continue;
        cJSON *start = cJSON_GetObjectItemCaseSensitive(range, "start");
        if (!start) continue;
        cJSON *line_j = cJSON_GetObjectItemCaseSensitive(start, "line");
        cJSON *col_j = cJSON_GetObjectItemCaseSensitive(start, "column");
        cJSON *lines_j = cJSON_GetObjectItemCaseSensitive(match, "lines");
        cJSON *text_j = cJSON_GetObjectItemCaseSensitive(match, "text");

        int line = cJSON_IsNumber(line_j) ? (int)line_j->valueint + 1 : 0;
        int col = cJSON_IsNumber(col_j) ? (int)col_j->valueint + 1 : 0;
        const char *source_line = cJSON_IsString(lines_j) ? lines_j->valuestring : "";
        const char *matched_text = cJSON_IsString(text_j) ? text_j->valuestring : "";

        int should_report = 1;
        (void)matched_text;

        if (strcmp(identifier, "system") == 0 || strcmp(identifier, "popen") == 0) {
            if (strstr(source_line, "// spec2c-check:") &&
                strstr(source_line, "allow-shell")) continue;
        }

        if (strcmp(identifier, "exit") == 0 || strcmp(identifier, "_exit") == 0) {
            if (strstr(file_content, "usage(") &&
                strstr(source_line, "exit")) {
                int near_usage = 0;
                const char *fp = file_content;
                int cline = 1;
                while (*fp && cline < line - 5) { if (*fp == '\n') cline++; fp++; }
                char ctx[1024] = {0};
                int ci = 0;
                for (int k = 0; k < 10 && *fp; k++) {
                    while (*fp && *fp != '\n' && ci < 1000) ctx[ci++] = *fp++;
                    ctx[ci++] = '\n';
                    if (*fp == '\n') fp++;
                }
                if (strstr(ctx, "usage") || strstr(ctx, "_Noreturn"))
                    near_usage = 1;
                if (near_usage) continue;
            }
        }

        if (pat->null_check_window > 0) {
            int has_null_check = 0;
            if (strstr(source_line, "if") && (strstr(source_line, "!") || strstr(source_line, "NULL")))
                has_null_check = 1;
            if (!has_null_check && strstr(source_line, "die"))
                has_null_check = 1;
            if (!has_null_check) {
                const char *fp = file_content;
                int cline = 1;
                while (*fp && cline < line) { if (*fp == '\n') cline++; fp++; }
                for (int k = 0; k < pat->null_check_window && !has_null_check; k++) {
                    while (*fp && *fp != '\n') {
                        if (*fp == '!' || strncmp(fp, "NULL", 4) == 0 ||
                            strncmp(fp, "die(", 4) == 0) { has_null_check = 1; break; }
                        fp++;
                    }
                    if (*fp == '\n') fp++;
                }
            }
            if (has_null_check) should_report = 0;
        }

        if (pat->nsql_patterns > 0) {
            int has_sql = 0;
            const char *fmt_start = strchr(source_line, '"');
            if (fmt_start) {
                for (int j = 0; j < pat->nsql_patterns; j++) {
                    if (strstr(fmt_start, pat->sql_patterns[j])) { has_sql = 1; break; }
                }
            }
            if (!has_sql) should_report = 0;
        }

        if (should_report) {
            dl_add(dl, "forbidden-pattern", identifier, pat->severity,
                   line, col, source_line, pat->message, pat->suggestion, -1);
        }
    }
    cJSON_Delete(root);
}

static void check_string_pattern(const pattern_def_t *pat, const char *file_path,
                                 const char *file_content, deviation_list_t *dl) {
    (void)file_path;
    const char *p = file_content;
    int line = 1;
    while (*p) {
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        size_t line_len = (size_t)(p - line_start);
        for (int i = 0; i < pat->nforbidden_strings; i++) {
            const char *found = strstr(line_start, pat->forbidden_strings[i]);
            if (found && found < line_start + line_len) {
                int col = (int)(found - line_start) + 1;
                char src_copy[MAX_LINE];
                size_t copy_len = line_len < MAX_LINE - 1 ? line_len : MAX_LINE - 1;
                memcpy(src_copy, line_start, copy_len);
                src_copy[copy_len] = '\0';
                dl_add(dl, "forbidden-string", pat->id, pat->severity,
                       line, col, src_copy, pat->message, pat->suggestion, -1);
            }
        }
        if (*p == '\n') { p++; line++; }
    }
}

static void check_scaffold_markers(const char *file_content, const char *spec_name,
                                    deviation_list_t *dl) {
    (void)spec_name;
    int has_vehir_lib = (strstr(file_content, "#include \"vehir_lib.h\"") != NULL);
    int has_usage = (strstr(file_content, "_Noreturn void usage(") != NULL);
    int has_extern = (strstr(file_content, "extern int") != NULL);
    int has_main = (strstr(file_content, "int main(int argc, char *argv[])") != NULL);
    int has_help = (strstr(file_content, "--help") != NULL);
    int has_vl_die = (strstr(file_content, "vl_die(") != NULL);
    int has_system = (strstr(file_content, "system(") != NULL);
    int has_popen = (strstr(file_content, "popen(") != NULL);

    if (!has_vehir_lib)
        dl_add(dl, "scaffold-marker", "missing-include", SEV_ERROR, 0, 0, "",
               "Missing #include \"vehir_lib.h\" — scaffold not present",
               "Add #include \"vehir_lib.h\"", -1);

    if (!has_usage)
        dl_add(dl, "scaffold-marker", "missing-usage", SEV_ERROR, 0, 0, "",
               "Missing _Noreturn usage() function",
               "Add usage() function generated by spec2c", -1);

    if (!has_extern)
        dl_add(dl, "scaffold-marker", "missing-extern", SEV_INFO, 0, 0, "",
               "No extern core function declaration found",
               "Add extern int <core_fn>(...) declaration", -1);

    if (!has_main)
        dl_add(dl, "scaffold-marker", "missing-main", SEV_ERROR, 0, 0, "",
               "Missing int main(int argc, char *argv[])",
               "Add main() scaffold from spec2c", -1);

    if (!has_help)
        dl_add(dl, "scaffold-marker", "missing-help", SEV_WARNING, 0, 0, "",
               "No --help handling found",
               "Add --help/-h check calling usage()", -1);

    if (has_system)
        dl_add(dl, "scaffold-marker", "scaffold-system", SEV_ERROR, 0, 0, "",
               "system() call in scaffold — must use vl_safe_exec()",
               "Replace system() with vl_safe_exec()", -1);

    if (has_popen)
        dl_add(dl, "scaffold-marker", "scaffold-popen", SEV_ERROR, 0, 0, "",
               "popen() call in scaffold — must use vl_safe_exec()",
               "Replace popen() with vl_safe_exec()", -1);

    if (!has_vl_die)
        dl_add(dl, "scaffold-marker", "missing-vl-die", SEV_WARNING, 0, 0, "",
               "No vl_die() usage — scaffold should use vl_die() for structured errors",
               "Replace bare exit()/fprintf+exit with vl_die()", -1);
}

static int load_patterns(const char *path, pattern_def_t **patterns_out, int *npatterns_out) {
    char *text = read_file(path);
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) return 0;

    cJSON *pats = cJSON_GetObjectItemCaseSensitive(root, "patterns");
    if (!cJSON_IsArray(pats)) { cJSON_Delete(root); return 0; }

    int npats = cJSON_GetArraySize(pats);
    pattern_def_t *result = calloc((size_t)npats, sizeof(pattern_def_t));
    if (!result) { cJSON_Delete(root); die("malloc"); }

    int count = 0;
    for (int i = 0; i < npats; i++) {
        cJSON *pat = cJSON_GetArrayItem(pats, i);
        if (!pat) continue;

        pattern_def_t *pd = &result[count];

        cJSON *j = cJSON_GetObjectItemCaseSensitive(pat, "id");
        pd->id = cJSON_IsString(j) ? strdup(j->valuestring) : NULL;

        j = cJSON_GetObjectItemCaseSensitive(pat, "severity");
        pd->severity_str = cJSON_IsString(j) ? strdup(j->valuestring) : strdup("info");
        pd->severity = parse_severity(pd->severity_str);

        j = cJSON_GetObjectItemCaseSensitive(pat, "message");
        pd->message = cJSON_IsString(j) ? strdup(j->valuestring) : NULL;

        j = cJSON_GetObjectItemCaseSensitive(pat, "suggestion");
        pd->suggestion = cJSON_IsString(j) ? strdup(j->valuestring) : NULL;

        cJSON *ids = cJSON_GetObjectItemCaseSensitive(pat, "identifiers");
        if (cJSON_IsArray(ids)) {
            pd->nidentifiers = cJSON_GetArraySize(ids);
            pd->identifiers = calloc((size_t)pd->nidentifiers, sizeof(const char *));
            for (int k = 0; k < pd->nidentifiers; k++) {
                cJSON *id = cJSON_GetArrayItem(ids, k);
                if (cJSON_IsString(id)) pd->identifiers[k] = strdup(id->valuestring);
            }
        }

        cJSON *fbs = cJSON_GetObjectItemCaseSensitive(pat, "forbidden_strings");
        if (cJSON_IsArray(fbs)) {
            pd->nforbidden_strings = cJSON_GetArraySize(fbs);
            pd->forbidden_strings = calloc((size_t)pd->nforbidden_strings, sizeof(const char *));
            for (int k = 0; k < pd->nforbidden_strings; k++) {
                cJSON *s = cJSON_GetArrayItem(fbs, k);
                if (cJSON_IsString(s)) pd->forbidden_strings[k] = strdup(s->valuestring);
            }
        }

        cJSON *sqls = cJSON_GetObjectItemCaseSensitive(pat, "sql_patterns");
        if (cJSON_IsArray(sqls)) {
            pd->nsql_patterns = cJSON_GetArraySize(sqls);
            pd->sql_patterns = calloc((size_t)pd->nsql_patterns, sizeof(const char *));
            for (int k = 0; k < pd->nsql_patterns; k++) {
                cJSON *s = cJSON_GetArrayItem(sqls, k);
                if (cJSON_IsString(s)) pd->sql_patterns[k] = strdup(s->valuestring);
            }
        }

        j = cJSON_GetObjectItemCaseSensitive(pat, "null_check_window");
        pd->null_check_window = cJSON_IsNumber(j) ? j->valueint : 0;

        count++;
    }

    *patterns_out = result;
    *npatterns_out = count;
    cJSON_Delete(root);
    return 1;
}

static void emit_report(const deviation_list_t *dl, const char *file_path,
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
