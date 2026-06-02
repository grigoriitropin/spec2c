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
    snprintf(guard, sizeof(guard), "%s_H", modname);
    for (char *p = guard; *p; p++) if (*p == '-') *p = '_';

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

void handle_ipm_spec_emit_code(const ipm_spec_t *spec, const char *out_path, int is_library) {
    if (is_library && out_path) {
        char hdr_path[4096];
        size_t olen = strlen(out_path);
        if (olen > 2 && !strcmp(out_path + olen - 2, ".c")) {
            memcpy(hdr_path, out_path, olen - 2);
            hdr_path[olen - 2] = '.'; hdr_path[olen - 1] = 'h'; hdr_path[olen] = 0;
        } else {
            snprintf(hdr_path, sizeof(hdr_path), "%s.h", out_path);
        }
        FILE *hdr = fopen(hdr_path, "w");
        if (hdr) {
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
    }

    const char *modname = extract_json_field_string_value(spec->meta, "module_name");
    if (modname[0] && out_path) {
        char hdr_path[4096];
        const char *slash = strrchr(out_path, '/');
        int dirlen = slash ? (int)(slash - out_path + 1) : 0;
        snprintf(hdr_path, sizeof(hdr_path), "%.*s%s.h", dirlen, out_path, modname);
        write_component_header_with_prototypes(spec, hdr_path);
    }

    FILE *out_fp = out_path ? fopen(out_path, "w") : stdout;
    if (!out_fp) report_fatal_error_and_exit("cannot open output file");

    if (modname[0]) {
        fprintf(out_fp, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n");
        fprintf(out_fp, "#include \"%s.h\"\n\n", modname);
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
        report_fatal_error_and_exit("spec uses template_definitions — raw C passthrough forbidden. All code must be generated from function_definitions (AST instructions).");

    compile_every_function_into_code(spec, out_fp, is_library);

    if (!is_library) {
        cJSON *func_defs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
        const char *entry = "main";
        if (func_defs && func_defs->child) entry = func_defs->child->string;
        fprintf(out_fp, "\n/* Auto-generated entry point */\n");
        fprintf(out_fp, "int main(int argc, char **argv) {\n");
        fprintf(out_fp, "    g_argc = argc;\n");
        fprintf(out_fp, "    g_argv = argv;\n");
        fprintf(out_fp, "    if (argc < 3) {\n");
        fprintf(out_fp, "        fprintf(stderr, \"Usage: %%s <input.ipm> <output.c>\\n\", argv[0]);\n");
        fprintf(out_fp, "        return 1;\n");
        fprintf(out_fp, "    }\n");
        fprintf(out_fp, "    return %s();\n", entry);
        fprintf(out_fp, "}\n");
    }

    if (out_path) fclose(out_fp);
}

char *resolve_template_file_from_base(const char *base, const char *file) {
    char *path = malloc(strlen(base) + strlen(file) + 2);
    if (!path) report_fatal_error_and_exit("malloc");
    sprintf(path, "%s/%s", base, file);
    return path;
}
