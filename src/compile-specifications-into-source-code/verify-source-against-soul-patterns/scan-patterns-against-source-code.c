// SPDX-License-Identifier: Apache-2.0
#include "share-check-types-and-declarations.h"


void match_identifier_against_ast_grep(const pattern_def_t *pat, const char *identifier,
                                     const char *sg_cmd, const char *file_path,
                                     const char *file_content, deviation_list_t *dl) {
    char *sg_output = execute_ast_grep_with_pattern(sg_cmd, file_path, identifier);
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
            if (strstr(source_line, "spec2c-check:") &&
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
            append_deviation_into_report_list(dl, "forbidden-pattern", identifier, pat->severity,
                   line, col, source_line, pat->message, pat->suggestion, -1);
        }
    }
    cJSON_Delete(root);
}

void match_string_against_file_content(const pattern_def_t *pat, const char *file_path,
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
                append_deviation_into_report_list(dl, "forbidden-string", pat->id, pat->severity,
                       line, col, src_copy, pat->message, pat->suggestion, -1);
            }
        }
        if (*p == '\n') { p++; line++; }
    }
}

void scan_for_scaffold_marker_deviations(const char *file_content, const char *spec_name,
                                    deviation_list_t *dl) {
    (void)spec_name;
    int has_vehir_lib = (strstr(file_content, "#include \"vehir-shared-abstraction-wrapper-code.h\"") != NULL);
    int has_usage = (strstr(file_content, "_Noreturn void usage(") != NULL);
    int has_extern = (strstr(file_content, "extern int") != NULL);
    int has_main = (strstr(file_content, "

static void extract_pattern_fields_from_json(cJSON *pat, pattern_def_t *pd) {
    cJSON *j = cJSON_GetObjectItemCaseSensitive(pat, "id");
    pd->id = cJSON_IsString(j) ? strdup(j->valuestring) : NULL;
    j = cJSON_GetObjectItemCaseSensitive(pat, "severity");
    pd->severity_str = cJSON_IsString(j) ? strdup(j->valuestring) : strdup("info");
    pd->severity = parse_severity_from_config_string(pd->severity_str);
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
}

int read_pattern_definitions_from_json(const char *path, pattern_def_t **patterns_out, int *npatterns_out) {
    char *text = read_entire_text_into_memory(path);
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) return 0;

    cJSON *pats = cJSON_GetObjectItemCaseSensitive(root, "patterns");
    if (!cJSON_IsArray(pats)) { cJSON_Delete(root); return 0; }

    int npats = cJSON_GetArraySize(pats);
    pattern_def_t *result = calloc((size_t)npats, sizeof(pattern_def_t));
    if (!result) { cJSON_Delete(root); terminate_with_error_message_output("malloc"); }

    int count = 0;
    for (int i = 0; i < npats; i++) {
        cJSON *pat = cJSON_GetArrayItem(pats, i);
        if (!pat) continue;
        extract_pattern_fields_from_json(pat, &result[count]);
        count++;
    }

    *patterns_out = result;
    *npatterns_out = count;
    cJSON_Delete(root);
    return 1;
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
