// SPDX-License-Identifier: Apache-2.0
// gen.c — AST-to-C code generation

#include "shared-type-declarations-across-modules/share-type-definitions-across-files.h"

void append_key_value_into_substitution(subst_t *subs, int *n, const char *k, const char *v) {
    if (*n >= SUBST_MAX) report_fatal_error_and_exit("too many substitutions");
    subs[*n].key = k;
    snprintf(subs[*n].val, VAL_SZ, "%s", v ? v : "");
    (*n)++;
}










void generate_code_from_ast_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type) {
    generate_code_via_dispatch_table(instructions, out, indent, return_type);
}

static const char *resolve_function_return_type_code(const char *ret) {
    if (!strcmp(ret, "i32")) return "int";
    if (!strcmp(ret, "str")) return "char *";
    if (!strcmp(ret, "i64")) return "long long";
    if (!ret[0]) return "void";
    if (!strcmp(ret, "i32")) return "int";
    if (!strcmp(ret, "str")) return "char *";
    if (!strcmp(ret, "i64")) return "long long";
    if (!strcmp(ret, "void")) return "void";
    if (!strcmp(ret, "int")) return "int";
    if (!strcmp(ret, "double")) return "double";
    if (!strcmp(ret, "boolean")) return "int";
    if (!strcmp(ret, "string") || !strcmp(ret, "char")) return "char *";
    if (!strcmp(ret, "file_handle")) return "FILE *";
    if (!strcmp(ret, "json_object")) return "cJSON *";
    if (!strcmp(ret, "json_array")) return "cJSON *";
    if (!strcmp(ret, "db_handle")) return "struct vehir_db *";
    if (!strcmp(ret, "subst_table")) return "subst_table *";
    if (!strcmp(ret, "i32")) return "int";
    if (!strcmp(ret, "str")) return "char *";
    if (!strcmp(ret, "i64")) return "long long";
    return "void";
}

static void emit_function_body_into_output(cJSON *fn, FILE *out, int is_library, int has_modname, const char *modname) {
    const char *name = fn->string;
    cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
    cJSON *body  = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
    if (!body || !cJSON_IsArray(body)) return;
    const char *ret = extract_json_field_string_value(fn, "return_type");
    if (modname && modname[0]) fprintf(out, "// @ipm:%s:%s\n", modname, name);
    if (name && !strcmp(name, "main"))
        fprintf(out, "int main(int argc, char **argv) {\n");
    else
        fprintf(out, "%s%s %s(", is_library ? "" : (has_modname ? "" : "static "),
            resolve_function_return_type_code(ret), name);
    if (params && cJSON_IsArray(params)) {
        for (int p = 0; p < cJSON_GetArraySize(params); p++) {
            cJSON *par = cJSON_GetArrayItem(params, p);
            const char *pn = extract_json_field_string_value(par, "parameter_name");
            const char *pt = extract_json_field_string_value(par, "parameter_type");
            if (p > 0) fprintf(out, ", ");
            fprintf(out, "%s %s", resolve_spec_type_into_lang(pt), pn);
        }
    }
    fprintf(out, ") {\n");
    generate_code_from_ast_instructions(body, out, 1, ret);
    fprintf(out, "}\n\n");
}

static void emit_extern_imports_into_output(cJSON *spec_meta, FILE *out) {
    cJSON *exts = cJSON_GetObjectItemCaseSensitive(spec_meta, "extern_imports");
    if (!exts || !cJSON_IsArray(exts)) return;
    for (int ei = 0; ei < cJSON_GetArraySize(exts); ei++) {
        cJSON *ext = cJSON_GetArrayItem(exts, ei);
        const char *ename = extract_json_field_string_value(ext, "name");
        const char *eret = extract_json_field_string_value(ext, "return_type");
        if (!ename[0]) continue;
        fprintf(out, "extern %s %s(",
            eret[0] ? resolve_spec_type_into_lang(eret) : "void", ename);
        cJSON *eargs = cJSON_GetObjectItemCaseSensitive(ext, "arguments");
        if (eargs && cJSON_IsArray(eargs)) {
            for (int aj = 0; aj < cJSON_GetArraySize(eargs); aj++) {
                cJSON *arg = cJSON_GetArrayItem(eargs, aj);
                const char *at = extract_json_field_string_value(arg, "type");
                const char *an = extract_json_field_string_value(arg, "name");
                if (aj > 0) fprintf(out, ", ");
                fprintf(out, "%s %s", resolve_spec_type_into_lang(at), an);
            }
        }
        fprintf(out, ");\n");
    }
}

void compile_every_function_into_code(const ipm_spec_t *spec, FILE *out, int is_library) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec->meta, "function_definitions");
    if (!funcs || !cJSON_IsObject(funcs)) return;
    cJSON *fn = NULL;
    const char *modname = extract_json_field_string_value(spec->meta, "module_name");
    int has_mod = modname[0] != 0;
    if (has_mod) {
        fprintf(out, "#include \"%s.h\"\n", modname);
    } else {
        fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n#include \"ipm_builtins.h\"\n");
    }
    fprintf(out, "\n");
    emit_extern_imports_into_output(spec->meta, out);
    fprintf(out, "\n");
    if (!has_mod) {
        fn = funcs->child;
        while (fn) {
            const char *name = fn->string;
            fprintf(out, "%s%s %s(", is_library ? "" : "static ",
                resolve_function_return_type_code(extract_json_field_string_value(fn, "return_type")), name);
            cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
            if (params && cJSON_IsArray(params)) {
                for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                    cJSON *par = cJSON_GetArrayItem(params, p);
                    if (p > 0) fprintf(out, ", ");
                    fprintf(out, "%s %s", resolve_spec_type_into_lang(
                        extract_json_field_string_value(par, "parameter_type")),
                        extract_json_field_string_value(par, "parameter_name"));
                }
            }
            fprintf(out, ");\n");
            fn = fn->next;
        }
        fprintf(out, "\n");
    }
    fn = funcs->child;
    while (fn) {
        emit_function_body_into_output(fn, out, is_library, has_mod, modname);
        fn = fn->next;
    }
}
