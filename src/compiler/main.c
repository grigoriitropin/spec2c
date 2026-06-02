// SPDX-License-Identifier: Apache-2.0
// main.c — entry point, CLI argument parsing, utility functions

#include "common_h/common.h"

const char *jstr(const cJSON *obj, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v || !cJSON_IsString(v)) return "";
    return v->valuestring;
}

_Noreturn void die(const char *msg) {
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

char *name_to_ident(const char *name) {
    char *s = strdup(name);
    if (!s) return NULL;
    for (char *p = s; *p; p++)
        if (*p == '-') *p = '_';
    return s;
}

char *read_file(const char *path) {
    FILE *f;
    if (strcmp(path, "-") == 0) f = stdin;
    else { f = fopen(path, "r"); if (!f) die("cannot open file"); }
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) die("malloc failed");
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *t = realloc(buf, cap); if (!t) { free(buf); die("realloc"); } buf = t; }
    }
    if (ferror(f)) { free(buf); die("read error"); }
    buf[len] = '\0';
    if (f != stdin) fclose(f);
    return buf;
}

int main(int argc, char *argv[]) {
    #ifdef SPEC2C_SRC_DIR
    enforce_structural_limits(SPEC2C_SRC_DIR);
    #endif

    const char *spec_path = NULL;
    const char *out_path = NULL;
    const char *base_dir = NULL;
    int check_mode = 0;
    const char *check_spec = NULL;
    int is_library = 0;

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
                "  --library      Generate .c + .h library (skip main), for linking into other programs\n");
            return 0;
        } else if (strcmp(argv[i], "--library") == 0) {
            is_library = 1;
        } else if (strcmp(argv[i], "--check") == 0) {
            check_mode = 1;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                spec_path = argv[++i];
            } else {
                die("missing file argument for --check");
            }
        } else if (strcmp(argv[i], "--spec") == 0) {
            if (++i >= argc) die("missing argument for --spec");
            check_spec = argv[i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc) die("missing argument for -o");
            out_path = argv[i];
        } else if (strcmp(argv[i], "--base") == 0) {
            if (++i >= argc) die("missing argument for --base");
            base_dir = argv[i];
        } else if (!spec_path) {
            spec_path = argv[i];
        } else {
            die("unexpected argument");
        }
    }

    if (check_mode) {
        if (!spec_path) die("file argument required for --check");
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
        return safe_exec(args);
    }

    if (!spec_path) die("spec file required");
    if (!base_dir) base_dir = ".";

    char *spec_text = read_file(spec_path);
    cJSON *spec_json = cJSON_Parse(spec_text);
    if (!spec_json) die("JSON parse error in spec file");

    if (spec_text) {
        int file_lines = 0;
        for (const char *p = spec_text; *p; p++) if (*p == '\n') file_lines++;
        cJSON *limits = cJSON_GetObjectItemCaseSensitive(spec_json, "structural_limits");
        int max_file_lines = 2000, max_funcs = 15, max_func_lines = 250;
        if (limits && cJSON_IsArray(limits)) {
            for (int li = 0; li < cJSON_GetArraySize(limits); li++) {
                cJSON *l = cJSON_GetArrayItem(limits, li);
                const char *ln = jstr(l, "limit_name");
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
            die(buf);
        }
        cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
        if (funcs && cJSON_IsObject(funcs)) {
            int nfuncs = cJSON_GetArraySize(funcs);
            if (nfuncs > max_funcs) {
                char buf[256];
                snprintf(buf, sizeof(buf), "too many functions: %d (max %d)", nfuncs, max_funcs);
                die(buf);
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
                    die(buf);
                }
                fn = fn->next;
            }
        }
    }

    cJSON *pkg_name = cJSON_GetObjectItemCaseSensitive(spec_json, "package_name");
    if (pkg_name && cJSON_IsString(pkg_name)) {
        ipm_spec_t ipm;
        ipm.meta = spec_json;
        ipm.name = pkg_name->valuestring;
        cJSON *tp = cJSON_GetObjectItemCaseSensitive(spec_json, "package_type");
        ipm.type = (tp && cJSON_IsString(tp)) ? tp->valuestring : "tool";
        ipm.desc = "generated by spec2c from .ipm specification";
        generate_from_ipm(&ipm, out_path, is_library);
        cJSON *r = cJSON_CreateObject();
        cJSON_AddBoolToObject(r, "ok", 1);
        cJSON_AddStringToObject(r, "output", out_path ? out_path : "(stdout)");
        char *js = cJSON_PrintUnformatted(r);
        printf("%s\n", js);
        free(js);
        cJSON_Delete(r);
        cJSON_Delete(spec_json);
        free(spec_text);
        return 0;
    }

    char *skel_path = resolve_template(base_dir, "skeleton.json");
    char *skel_text = read_file(skel_path);
    free(skel_path);
    cJSON *skel = cJSON_Parse(skel_text);
    free(skel_text);
    if (!skel) die("JSON parse error in skeleton.json");

    spec_t spec;
    parse_spec_from_cjson(spec_json, &spec);
    free(spec_text);

    subst_t subs[SUBST_MAX];
    int nsubs = 0;
    compute_substs(&spec, subs, &nsubs);
    int has_config = spec.nconfig_keys > 0;
    int has_db = spec.has_db;

    cJSON *sections = cJSON_GetObjectItemCaseSensitive(skel, "sections");
    if (!cJSON_IsArray(sections)) die("skeleton.json: missing \"sections\" array");

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) die("cannot open output file");
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
        char *tpath = resolve_template(base_dir, file->valuestring);
        char *tmpl = read_file(tpath);
        free(tpath);
        char *result = subst_apply(tmpl, subs, nsubs);
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
    cJSON_Delete(skel);
    free((void *)spec.name);
    free((void *)spec.ident);
    free((void *)spec.description);
    free((void *)spec.core_function);
    free(spec.config_keys_str);
    return 0;
}
