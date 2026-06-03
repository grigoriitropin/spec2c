// SPDX-License-Identifier: Apache-2.0
// pipeline.c — .ipm pipeline: header generation, output emission, auto-main

#include "shared-type-declarations-across-modules/share-type-definitions-across-files.h"

static void emit_function_prototypes_into_header(FILE *hdr, cJSON *funcs) {
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn = funcs->child;
    while (fn) {
        const char *name = fn->string;
        const char *ret = extract_json_field_string_value(fn, "return_type");
        const char *ret_c = "void";
        if (ret[0]) {
            if (!strcmp(ret, "void")) ret_c = "void";
            else if (!strcmp(ret, "int")) ret_c = "int";
            else if (!strcmp(ret, "string") || !strcmp(ret, "char")) ret_c = "char *";
            else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
            else if (!strcmp(ret, "json_array")) ret_c = "cJSON *";
            else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
            else if (!strcmp(ret, "string_buffer")) ret_c = "string_buffer *";
        }
        fprintf(hdr, "%s %s(", ret_c, name);
        cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
        if (params && cJSON_IsArray(params)) {
            for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                cJSON *par = cJSON_GetArrayItem(params, p);
                const char *pn = extract_json_field_string_value(par, "parameter_name");
                const char *pt = extract_json_field_string_value(par, "parameter_type");
                if (p > 0) fprintf(hdr, ", ");
                fprintf(hdr, "%s %s", resolve_spec_type_into_lang(pt), pn);
            }
        }
        fprintf(hdr, ");\n");
        fn = fn->next;
    }
}

void write_component_header_with_prototypes(const ipm_spec_t *spec, const char *hdr_path) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    const char *modname = extract_json_field_string_value(spec->meta, "module_name");
    if (!modname[0] || !hdr_path) return;

    FILE *hdr = fopen(hdr_path, "w");
    if (!hdr) report_fatal_error_and_exit("cannot open header file");

    char guard[256];
    change_every_hyphen_into_underscore(guard, modname);
    strcat(guard, "_H");

    fprintf(hdr, "/* Auto-generated from %s.ipm */\n", modname);
    fprintf(hdr, "#ifndef %s\n#define %s\n\n", guard, guard);
    fprintf(hdr, "#include \"ipm_builtins.h\"\n");

    cJSON *imports = cJSON_GetObjectItemCaseSensitive(spec->meta, "imports");
    if (imports && cJSON_IsArray(imports)) {
        for (int i = 0; i < cJSON_GetArraySize(imports); i++) {
            cJSON *imp = cJSON_GetArrayItem(imports, i);
            if (cJSON_IsString(imp))
                fprintf(hdr, "#include \"%s.h\"\n", imp->valuestring);
        }
    }
    fprintf(hdr, "\n");

    emit_function_prototypes_into_header(hdr, funcs);

    fprintf(hdr, "\n#endif /* %s_H */\n", guard);
    fclose(hdr);
}

static void emit_ipm_component_header_file(const ipm_spec_t *spec, const char *out_path) {
    char hdr_path[4096];
    size_t olen = strlen(out_path);
    if (olen > 2 && !strcmp(out_path + olen - 2, ".c")) {
        memcpy(hdr_path, out_path, olen - 2);
        hdr_path[olen - 2] = '.'; hdr_path[olen - 1] = 'h'; hdr_path[olen] = 0;
    } else {
        snprintf(hdr_path, sizeof(hdr_path), "%s.h", out_path);
    }
    FILE *hdr = fopen(hdr_path, "w");
    if (!hdr) return;
    fprintf(hdr, "/* Auto-generated library header */\n");
    fprintf(hdr, "#ifndef IPM_LIB_H\n#define IPM_LIB_H\n\n");
    fprintf(hdr, "#include \"ipm_builtins.h\"\n\n");
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (funcs && cJSON_IsObject(funcs)) {
        cJSON *fn = funcs->child;
        while (fn) {
            const char *name = fn->string;
            fprintf(hdr, "%s %s(", resolve_spec_type_into_lang(extract_json_field_string_value(fn, "return_type")), name);
            cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    if (p > 0) fprintf(hdr, ", ");
                    fprintf(hdr, "%s %s", resolve_spec_type_into_lang(extract_json_field_string_value(par, "parameter_type")), extract_json_field_string_value(par, "parameter_name"));
                }
            }
            fprintf(hdr, ");\n");
            fn = fn->next;
        }
    }
    fprintf(hdr, "\n#endif /* IPM_LIB_H */\n");
    fclose(hdr);
}

static void generate_auto_entry_point_code(FILE *out_fp, const ipm_spec_t *spec) {
    cJSON *func_defs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!func_defs || !cJSON_IsObject(func_defs)) return;

    /* Skip if function_definitions already contains a "main" */
    cJSON *fn = func_defs->child;
    while (fn) {
        if (fn->string && !strcmp(fn->string, "main")) return;
        fn = fn->next;
    }

    const char *entry = func_defs->child ? func_defs->child->string : "main";
    fprintf(out_fp, "\n/* Auto-generated entry point */\n");
    fprintf(out_fp, "int main(int argc, char **argv) {\n");
    fprintf(out_fp, "    g_argc = argc;\n    g_argv = argv;\n");
    fprintf(out_fp, "    if (argc < 3) {\n");
    fprintf(out_fp, "        fprintf(stderr, \"Usage: %%s <input.ipm> <output.c>\\n\", argv[0]);\n");
    fprintf(out_fp, "        return 1;\n    }\n");
    fprintf(out_fp, "    return %s();\n}\n", entry);
}

static void change_every_hyphen_into_underscore(char *dst, const char *src) {
    int i = 0;
    while (src[i]) {
        if (src[i] == '-')
            dst[i] = '_';
        else
            dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void handle_ipm_spec_emit_code(const ipm_spec_t *spec, const char *out_path, int is_library) {
    if (is_library && out_path)
        emit_ipm_component_header_file(spec, out_path);

    const char *modname = extract_json_field_string_value(spec->meta, "module_name");
    if (modname[0] && out_path) {
        char hdr_path[4096];
        char cmod[256];
        change_every_hyphen_into_underscore(cmod, modname);
        const char *slash = strrchr(out_path, '/');
        int dirlen = slash ? (int)(slash - out_path + 1) : 0;
        snprintf(hdr_path, sizeof(hdr_path), "%.*s%s.h", dirlen, out_path, cmod);
        write_component_header_with_prototypes(spec, hdr_path);
    }

    FILE *out_fp = out_path ? fopen(out_path, "w") : stdout;
    if (!out_fp) report_fatal_error_and_exit("cannot open output file");

    if (modname[0]) {
        char cname[256];
        change_every_hyphen_into_underscore(cname, modname);
        fprintf(out_fp, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n");
        fprintf(out_fp, "#include \"%s.h\"\n\n", cname);
    }

    subst_t subs[SUBST_MAX]; int nsubs = 0;
    append_key_value_into_substitution(subs, &nsubs, "package_name", spec->name);
    append_key_value_into_substitution(subs, &nsubs, "package_type", spec->type);
    append_key_value_into_substitution(subs, &nsubs, "package_description", spec->desc ? spec->desc : "No description");
    cJSON *cfg = cJSON_GetObjectItemCaseSensitive(spec->meta, "configuration_keys");
    int has_config = cfg && cJSON_IsArray(cfg) && cJSON_GetArraySize(cfg) > 0;
    append_key_value_into_substitution(subs, &nsubs, "has_configuration", has_config ? "1" : "0");
    append_key_value_into_substitution(subs, &nsubs, "compiler_includes", "");
    append_key_value_into_substitution(subs, &nsubs, "compiler_function_implementations", "");
    append_key_value_into_substitution(subs, &nsubs, "argument_parsing_logic", "if (argc < 2) print_usage_and_exit(argv[0]);");
    append_key_value_into_substitution(subs, &nsubs, "pipeline_initialization", "fprintf(stderr, \"gen\\n\");");
    append_key_value_into_substitution(subs, &nsubs, "pipeline_execution", "fprintf(stderr, \"done\\n\");");
    append_key_value_into_substitution(subs, &nsubs, "ipm_metadata_header", "/* IPM Generated Code — DO NOT EDIT */");

    cJSON *templates = cJSON_GetObjectItemCaseSensitive(spec->meta, "template_definitions");
    if (templates && cJSON_IsObject(templates) && templates->child)
        report_fatal_error_and_exit("spec uses template_definitions — raw C passthrough forbidden.");

    compile_every_function_into_code(spec, out_fp, is_library);

    if (!is_library)
        generate_auto_entry_point_code(out_fp, spec);

    if (out_fp != stdout) fclose(out_fp);
}

char *resolve_template_file_from_base(const char *base, const char *file) {
    char *path = malloc(strlen(base) + strlen(file) + 2);
    if (!path) report_fatal_error_and_exit("malloc");
    sprintf(path, "%s/%s", base, file);
    return path;
}