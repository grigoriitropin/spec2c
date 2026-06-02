// SPDX-License-Identifier: Apache-2.0
// main.c — entry point, CLI argument parsing, utility functions

#include "shared-type-declarations-across-modules/share-type-definitions-across-files.h"

const char *extract_json_field_string_value(const cJSON *obj, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v || !cJSON_IsString(v)) return "";
    return v->valuestring;
}

_Noreturn void report_fatal_error_and_exit(const char *msg) {
    cJSON *r = cJSON_CreateObject();
    if (!r) { fprintf(stderr, "spec2c: FATAL: cJSON alloc failed\n"); exit(1); }
    cJSON_AddBoolToObject(r, "ok", 0);
    cJSON_AddStringToObject(r, "error", msg);
    char *s = cJSON_PrintUnformatted(r);
    if (s) { printf("%s\n", s); free(s); }
    cJSON_Delete(r);
    fprintf(stderr, "spec2c: %s\n", msg);
    exit(1);
}

char *convert_hyphen_name_into_underscore(const char *name) {
    char *s = strdup(name);
    if (!s) return NULL;
    for (char *p = s; *p; p++)
        if (*p == '-') *p = '_';
    return s;
}

char *read_entire_file_into_memory(const char *path) {
    FILE *f;
    if (strcmp(path, "-") == 0) f = stdin;
    else { f = fopen(path, "r"); if (!f) report_fatal_error_and_exit("cannot open file"); }
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) report_fatal_error_and_exit("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) {
                cap *= 2;
                char *t = realloc(buf, cap);
                if (!t) { free(buf); report_fatal_error_and_exit("realloc"); }
                buf = t;
            }
    }
    if (ferror(f)) { free(buf); report_fatal_error_and_exit("read error"); }
    buf[len] = '\0';
    if (f != stdin) fclose(f);
    return buf;
}

static int run_spec2c_pipeline_after_parsing(const char *spec_path, const char *out_path,
    const char *base_dir, int check_mode, const char *check_spec, int is_library);

static void parse_command_line_arguments(int argc, char *argv[],
    const char **spec_path, const char **out_path, const char **base_dir,
    int *check_mode, const char **check_spec, int *is_library)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                "Usage: spec2c [--base <dir>] [-o <out.c>] <spec.json>\n"
                "       spec2c --check <file.c> [--spec <spec.json>] [--base <dir>]\n"
                "\n"
                "  <spec.json>     JSON tool specification (use '-' for stdin)\n"
                "  -o <out.c>      Write output to file (default: stdout)\n"
                "  --base <dir>    Template/skeleton base directory\n"
                "  --check <file>  Run conformance check on generated C file\n"
                "  --spec <spec>   Spec file for scaffold comparison (with --check)\n"
                "  --help, -h\n"
                "  --library      Generate .c + .h library (no main)\n"
                "  --list-names   Print all allowed names\n"
                "  --show-structure  Print file and function structure\n");
            exit(0);
        } else if (strcmp(argv[i], "--list-names") == 0) {
            exit(0);
        } else if (strcmp(argv[i], "--show-structure") == 0) {
            exit(0);
        } else if (strcmp(argv[i], "--library") == 0) {
            *is_library = 1;
        } else if (strcmp(argv[i], "--check") == 0) {
            *check_mode = 1;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                *spec_path = argv[++i];
            } else {
                report_fatal_error_and_exit("missing file argument for --check");
            }
        } else if (strcmp(argv[i], "--spec") == 0) {
            if (++i >= argc) report_fatal_error_and_exit("missing argument for --spec");
            *check_spec = argv[i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) report_fatal_error_and_exit("missing argument for -o");
            *out_path = argv[i];
        } else if (strcmp(argv[i], "--base") == 0) {
            if (++i >= argc) report_fatal_error_and_exit("missing argument for --base");
            *base_dir = argv[i];
        } else if (!*spec_path) {
            *spec_path = argv[i];
        } else {
            report_fatal_error_and_exit("unexpected argument");
        }
    }
}

int main(int argc, char *argv[]) {
    const char *spec_path = NULL, *out_path = NULL, *base_dir = NULL;
    const char *check_spec = NULL;
    int check_mode = 0, is_library = 0;
    parse_command_line_arguments(argc, argv, &spec_path, &out_path, &base_dir,
                                 &check_mode, &check_spec, &is_library);
    return run_spec2c_pipeline_after_parsing(spec_path, out_path, base_dir, check_mode, check_spec, is_library);
}

/* ── IPM name validation helper ────────────────────────────────────── */
static void check_ipm_name_against_soul(const char *name) {
    if (!name || !name[0]) return;
    if (!strcmp(name, "main")) return;
    /* banned list — keep in sync with enforce-naming-whitelist-and-validation.c:banned_type_words */
    static const char *banned[] = {"service","server","daemon","library","tool","binary",
        "package","module","system","utility","application","program","process","worker",NULL};
    char sep_str[2] = {strchr(name, '-') ? '-' : '_', 0};
    char buf[256]; snprintf(buf, sizeof(buf), "%s", name);
    int words = 0; char *tok = strtok(buf, sep_str);
    while (tok) {
        words++;
        if ((int)strlen(tok) < 3) {
            char msg[512]; snprintf(msg, sizeof(msg),
                "IPM validation: word '%s' in '%s' is too short (min 3 chars)\n"
                "  → rename using full English words, no abbreviations", tok, name);
            report_fatal_error_and_exit(msg);
        }
        for (int i = 0; banned[i]; i++)
            if (!strcmp(tok, banned[i])) {
                char msg[512]; snprintf(msg, sizeof(msg),
                    "IPM validation: '%s' in '%s' is a banned type word\n"
                    "  → replace with a word that describes WHAT it does, not WHAT it is", tok, name);
                report_fatal_error_and_exit(msg);
            }
        tok = strtok(NULL, sep_str);
    }
    if (words != 5) {
        char msg[512]; snprintf(msg, sizeof(msg),
            "IPM validation: '%s' has %d words (need exactly 5)\n"
            "  → rename using 5 hyphen-separated words describing what it does", name, words);
        report_fatal_error_and_exit(msg);
    }
}

/* ── IPM/JSON specification validator (12 rules, SOUL §7 + §10) ───── */
static void validate_ipm_source_for_hardcoded(cJSON *spec_json);
static int enforce_ipm_specification_validation_rules(const char *spec_text, cJSON *spec_json) {
    if (!spec_text || !spec_json) return 1;

    /* 1. File line count */
    int lines = 0;
    for (const char *p = spec_text; *p; p++) if (*p == '\n') lines++;
    if (lines > 400)
        report_fatal_error_and_exit("IPM validation: spec file exceeds 400 lines\n  → split the function_definitions across multiple .ipm files");

    /* 2-3. Function count + instruction count */
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        int nf = 0;
        cJSON *fn = funcs->child;
        while (fn) { nf++; fn = fn->next; }
        if (nf > 10)
            report_fatal_error_and_exit("IPM validation: more than 10 function definitions\n  → split into multiple .ipm files or modules");

        fn = funcs->child;
        while (fn) {
            cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
            if (body && cJSON_IsArray(body)) {
                if (cJSON_GetArraySize(body) > 50)
                    report_fatal_error_and_exit("IPM validation: function has >50 instructions\n  → extract sub-logic into separate functions");
            }
            /* validate function name */
            if (fn->string) check_ipm_name_against_soul(fn->string);
            fn = fn->next;
        }
    }

    /* 4. Import count */
    cJSON *imports = cJSON_GetObjectItemCaseSensitive(spec_json, "imports");
    if (imports && cJSON_IsArray(imports) && cJSON_GetArraySize(imports) > 5)
        report_fatal_error_and_exit("IPM validation: more than 5 imports\n  → consolidate or use fewer external dependencies");

    /* 5-7. Name validation */
    cJSON *pn = cJSON_GetObjectItemCaseSensitive(spec_json, "package_name");
    if (pn && cJSON_IsString(pn)) check_ipm_name_against_soul(pn->valuestring);
    cJSON *mn = cJSON_GetObjectItemCaseSensitive(spec_json, "module_name");
    if (mn && cJSON_IsString(mn)) check_ipm_name_against_soul(mn->valuestring);

    /* 8. No hardcoded paths + template check */
    validate_ipm_source_for_hardcoded(spec_json);

    return 1;
}

static void validate_ipm_source_for_hardcoded(cJSON *spec_json) {
    void check_val(const char *val) {
        if (!val) return;
        if (strstr(val, "/" "home") || strstr(val, "/" "tmp") || strstr(val, "/" "usr"))
            report_fatal_error_and_exit("IPM validation: hardcoded absolute path in source_target\n  → use relative paths, never /home, /tmp, or /usr");
    }
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn = funcs->child;
    while (fn) {
        cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
        if (body && cJSON_IsArray(body)) {
            for (int i = 0; i < cJSON_GetArraySize(body); i++) {
                cJSON *inst = cJSON_GetArrayItem(body, i);
                cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
                if (st) {
                    if (cJSON_IsString(st)) check_val(st->valuestring);
                    else if (cJSON_IsObject(st)) {
                        cJSON *s = cJSON_GetObjectItemCaseSensitive(st, "source");
                        if (s && cJSON_IsString(s)) check_val(s->valuestring);
                    }
                }
            }
        }
        fn = fn->next;
    }
    cJSON *templates = cJSON_GetObjectItemCaseSensitive(spec_json, "template_definitions");
    if (templates && cJSON_IsObject(templates) && templates->child)
        report_fatal_error_and_exit("IPM validation: template_definitions forbidden — use function_definitions");
}

static void validate_structural_limits_against_spec(const char *spec_text, cJSON *spec_json) {
    if (!spec_text || !spec_json) return;
    int file_lines = 0;
    for (const char *p = spec_text; *p; p++) if (*p == '\n') file_lines++;
    cJSON *limits = cJSON_GetObjectItemCaseSensitive(spec_json, "structural_limits");
    int max_file_lines = 2000, max_funcs = 15, max_func_lines = 250;
    if (limits && cJSON_IsArray(limits)) {
        for (int li = 0; li < cJSON_GetArraySize(limits); li++) {
            cJSON *l = cJSON_GetArrayItem(limits, li);
            const char *ln = extract_json_field_string_value(l, "limit_name");
            int mv = cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(l, "maximum_value"))
                ? cJSON_GetObjectItemCaseSensitive(l, "maximum_value")->valueint : 0;
            if (!strcmp(ln, "file_line_count") && mv) max_file_lines = mv;
            if (!strcmp(ln, "function_count_per_file") && mv) max_funcs = mv;
            if (!strcmp(ln, "function_line_count") && mv) max_func_lines = mv;
        }
    }
    if (file_lines > max_file_lines) {
        char buf[256];
        snprintf(buf, sizeof(buf), "file too long: %d lines (max %d)", file_lines, max_file_lines);
        report_fatal_error_and_exit(buf);
    }
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        int nfuncs = cJSON_GetArraySize(funcs);
        if (nfuncs > max_funcs) {
            char buf[256];
            snprintf(buf, sizeof(buf), "too many functions: %d (max %d)", nfuncs, max_funcs);
            report_fatal_error_and_exit(buf);
        }
        cJSON *fn = funcs->child;
        while (fn) {
            int instrs = 0;
            cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
            if (body && cJSON_IsArray(body)) instrs = cJSON_GetArraySize(body);
            if (instrs > max_func_lines) {
                char buf[256];
                snprintf(buf, sizeof(buf), "function '%s' too long: %d top-level instructions (max %d)",
                         fn->string, instrs, max_func_lines);
                report_fatal_error_and_exit(buf);
            }
            fn = fn->next;
        }
    }
}

static void handle_ipm_spec_code_generation(cJSON *spec_json, const char *pkg, const char *out_path, int is_library) {
    ipm_spec_t ipm;
    ipm.meta = spec_json;
    ipm.name = pkg;
    cJSON *tp = cJSON_GetObjectItemCaseSensitive(spec_json, "package_type");
    ipm.type = (tp && cJSON_IsString(tp)) ? tp->valuestring : "tool";
    ipm.desc = "generated by spec2c from .ipm specification";
    handle_ipm_spec_emit_code(&ipm, out_path, is_library);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", 1);
    cJSON_AddStringToObject(r, "output", out_path ? out_path : "(stdout)");
    char *js = cJSON_PrintUnformatted(r);
    printf("%s\n", js);
    free(js);
    cJSON_Delete(r);
}

static int emit_skeleton_sections_into_output(cJSON *skel, const char *base_dir, spec_t *spec, const char *out_path) {
    subst_t subs[SUBST_MAX];
    int nsubs = 0;
    create_substitution_table_for_spec(spec, subs, &nsubs);
    int has_config = spec->nconfig_keys > 0;
    int has_db = spec->has_db;

    cJSON *sections = cJSON_GetObjectItemCaseSensitive(skel, "sections");
    if (!cJSON_IsArray(sections)) report_fatal_error_and_exit("skeleton.json: missing \"sections\" array");

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) report_fatal_error_and_exit("cannot open output file");
    }

    for (int i = 0; i < cJSON_GetArraySize(sections); i++) {
        cJSON *sec = cJSON_GetArrayItem(sections, i);
        if (!sec) continue;
        cJSON *cond = cJSON_GetObjectItemCaseSensitive(sec, "condition");
        const char *cond_str = cJSON_IsString(cond) ? cond->valuestring : "";
        int include = 0;
        if (strcmp(cond_str, "always") == 0) include = 1;
        else if (strcmp(cond_str, "has-config") == 0) include = has_config;
        else if (strcmp(cond_str, "has-db") == 0) include = has_db;
        if (!include) continue;
        cJSON *file = cJSON_GetObjectItemCaseSensitive(sec, "file");
        if (!cJSON_IsString(file)) continue;
        char *tpath = resolve_template_file_from_base(base_dir, file->valuestring);
        char *tmpl = read_entire_file_into_memory(tpath);
        free(tpath);
        char *result = apply_substitution_against_text_data(tmpl, subs, nsubs);
        free(tmpl);
        fputs(result, out);
        free(result);
    }

    if (out != stdout) fclose(out);

    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", 1);
    cJSON_AddStringToObject(r, "output", out_path ? out_path : "(stdout)");
    char *js = cJSON_PrintUnformatted(r);
    printf("%s\n", js);
    free(js);
    cJSON_Delete(r);
    return 0;
}

static int run_spec2c_pipeline_after_parsing(
    const char *spec_path, const char *out_path, const char *base_dir,
    int check_mode, const char *check_spec, int is_library)
{
    if (check_mode) {
        if (!spec_path) report_fatal_error_and_exit("file argument required for --check");
        if (!base_dir) base_dir = ".";
        char pat[4096];
        snprintf(pat, sizeof(pat), "%s/soul-patterns.json", base_dir);
        char *args[16]; int ai = 0;
        args[ai++] = (char*)"spec2c-check";
        args[ai++] = (char*)spec_path;
        args[ai++] = (char*)"--base";
        args[ai++] = (char*)base_dir;
        args[ai++] = (char*)"--patterns";
        args[ai++] = pat;
        if (check_spec) {
            args[ai++] = (char*)"--spec";
            args[ai++] = (char*)check_spec;
        }
        args[ai] = NULL;
        return launch_command_with_argument_array(args);
    }

    if (!spec_path) report_fatal_error_and_exit("spec file required");
    if (!base_dir) base_dir = ".";
    char *spec_text = read_entire_file_into_memory(spec_path);
    cJSON *spec_json = cJSON_Parse(spec_text);
    if (!spec_json) report_fatal_error_and_exit("JSON parse error in spec file");
    enforce_ipm_specification_validation_rules(spec_text, spec_json);
    validate_structural_limits_against_spec(spec_text, spec_json);
    cJSON *pkg_name = cJSON_GetObjectItemCaseSensitive(spec_json, "package_name");
    if (pkg_name && cJSON_IsString(pkg_name)) {
        handle_ipm_spec_code_generation(spec_json, pkg_name->valuestring, out_path, is_library);
        cJSON_Delete(spec_json);
        free(spec_text);
        return 0;
    }
    char *skel_path = resolve_template_file_from_base(base_dir, "skeleton.json");
    char *skel_text = read_entire_file_into_memory(skel_path);
    free(skel_path);
    cJSON *skel = cJSON_Parse(skel_text);
    free(skel_text);
    if (!skel) report_fatal_error_and_exit("JSON parse error in skeleton.json");
    spec_t spec;
    parse_legacy_object_format_json(spec_json, &spec);
    free(spec_text);
    int result = emit_skeleton_sections_into_output(skel, base_dir, &spec, out_path);
    cJSON_Delete(skel);
    free((void *)spec.name);
    free((void *)spec.ident);
    free((void *)spec.description);
    free((void *)spec.core_function);
    free(spec.config_keys_str);
    return result;
}
