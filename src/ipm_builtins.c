// SPDX-License-Identifier: Apache-2.0
// ipm_builtins.c — deterministic runtime library for spec2c-generated code
#include "ipm_builtins.h"

/* ── I/O primitives ──────────────────────────────────────────────────── */

string read_file_to_string(const char *path) {
    FILE *f = (!path || !path[0]) ? stdin : fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) { if (f != stdin) fclose(f); return NULL; }
    size_t n = fread(buffer, 1, (size_t)size, f);
    buffer[n] = '\0';
    if (f != stdin) fclose(f);
    return buffer;
}

void write_string_to_file(const char *path, string content) {
    if (!path || !content) return;
    FILE *f = fopen(path, "a");  /* append — caller manages file lifecycle */
    if (!f) return;
    fputs(content, f);
    fclose(f);
}

/* ── JSON ─────────────────────────────────────────────────────────────── */

json_object* parse_json_string(string content) {
    if (!content) return NULL;
    return cJSON_Parse(content);
}

/* ── Hash table (sorted array — deterministic iteration) ──────────────── */

subst_table* create_hash_table(void) {
    subst_table *t = malloc(sizeof(subst_table));
    t->entries = malloc(16 * sizeof(subst_entry));
    t->count = 0;
    t->capacity = 16;
    return t;
}

static int compare_entries(const void *a, const void *b) {
    return strcmp(((const subst_entry *)a)->key, ((const subst_entry *)b)->key);
}

void hash_table_insert(subst_table *table, const char *key, const char *value) {
    if (!table || !key || !value) return;
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0) {
            free(table->entries[i].value);
            table->entries[i].value = strdup(value);
            return;
        }
    }
    if (table->count == table->capacity) {
        table->capacity *= 2;
        table->entries = realloc(table->entries,
            (size_t)table->capacity * sizeof(subst_entry));
    }
    table->entries[table->count].key = strdup(key);
    table->entries[table->count].value = strdup(value);
    table->count++;
    qsort(table->entries, (size_t)table->count, sizeof(subst_entry), compare_entries);
}

const char* hash_table_lookup(const subst_table *table, const char *key) {
    if (!table || !key) return NULL;
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0)
            return table->entries[i].value;
    }
    return NULL;
}

void hash_table_free(subst_table *table) {
    if (!table) return;
    for (int i = 0; i < table->count; i++) {
        free(table->entries[i].key);
        free(table->entries[i].value);
    }
    free(table->entries);
    free(table);
}

int hash_table_count(const subst_table *table) {
    return table ? table->count : 0;
}

/* ── String substitution ──────────────────────────────────────────────── */

string string_substitute(string template_str, const subst_table *table) {
    if (!template_str) return NULL;
    size_t rcap = strlen(template_str) * 2 + 256;
    char *result = malloc(rcap);
    if (!result) return NULL;
    size_t rpos = 0;
    const char *p = template_str;

    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            const char *key = p + 2;
            const char *end = strstr(key, "}}");
            if (!end) { result[rpos++] = *p++; continue; }
            size_t klen = (size_t)(end - key);
            char *kbuf = malloc(klen + 1);
            if (!kbuf) { result[rpos++] = *p++; continue; }
            memcpy(kbuf, key, klen); kbuf[klen] = '\0';
            const char *val = hash_table_lookup(table, kbuf);
            free(kbuf);
            if (val) {
                size_t vlen = strlen(val);
                while (rpos + vlen >= rcap) {
                    rcap *= 2; result = realloc(result, rcap);
                }
                memcpy(result + rpos, val, vlen);
                rpos += vlen;
            }
            p = end + 2;
        } else {
            if (rpos + 1 >= rcap) { rcap *= 2; result = realloc(result, rcap); }
            result[rpos++] = *p++;
        }
    }
    result[rpos] = '\0';
    return result;
}

/* ── Error handling ───────────────────────────────────────────────────── */

void die_builtin(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg ? msg : "unknown error");
    exit(1);
}

void print_error_to_stderr(const char *msg) {
    fprintf(stderr, "error: %s\n", msg ? msg : "unknown");
}

void exit_process(int code) {
    exit(code);
}

/* ── AST-to-C compiler (Phase 2b built-in) ───────────────────────────── */

static const char *builtin_vartype_to_c(const char *t) {
    if (!strcmp(t, "string")) return "char *";
    if (!strcmp(t, "int")) return "int";
    if (!strcmp(t, "float")) return "double";
    if (!strcmp(t, "boolean")) return "int";
    if (!strcmp(t, "json_object")) return "cJSON *";
    if (!strcmp(t, "json_array")) return "cJSON *";
    if (!strcmp(t, "db_handle")) return "struct vehir_db *";
    if (!strcmp(t, "subst_table")) return "subst_table *";
    return "void *";
}

static const char *builtin_jstr(const cJSON *obj, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!v || !cJSON_IsString(v)) return NULL;
    return v->valuestring;
}

static void builtin_compile_instructions(cJSON *instructions, FILE *out, int indent, const char *return_type) {
    if (!cJSON_IsArray(instructions)) return;
    for (int ii = 0; ii < cJSON_GetArraySize(instructions); ii++) {
        cJSON *inst = cJSON_GetArrayItem(instructions, ii);
        if (!inst) continue;
        cJSON *it = cJSON_GetObjectItemCaseSensitive(inst, "instruction_type");
        if (!cJSON_IsString(it)) continue;
        const char *type = it->valuestring;
        for (int s = 0; s < indent; s++) fputs("  ", out);

        if (!strcmp(type, "access_json_field")) {
            const char *vn = builtin_jstr(inst, "variable_name");
            const char *vt = builtin_jstr(inst, "variable_type");
            const char *so = builtin_jstr(inst, "source_object");
            const char *fn = builtin_jstr(inst, "field_name");
            if (vn && so && fn) {
                if (vt && (!strcmp(vt, "string") || !strcmp(vt, "char"))) {
                    fprintf(out, "const char *%s = cJSON_GetObjectItemCaseSensitive(%s,\"%s\") ? cJSON_GetObjectItemCaseSensitive(%s,\"%s\")->valuestring : \"\";\n",
                            vn, so, fn, so, fn);
                } else {
                    fprintf(out, "cJSON *%s = cJSON_GetObjectItemCaseSensitive(%s, \"%s\");\n", vn, so, fn);
                }
            }
        } else if (!strcmp(type, "function_invocation")) {
            const char *fn = builtin_jstr(inst, "invocation_name");
            const char *rv = builtin_jstr(inst, "result_assignment_variable");
            const char *rt = builtin_jstr(inst, "result_type");
            cJSON *args = cJSON_GetObjectItemCaseSensitive(inst, "invocation_arguments");
            if (fn) {
                if (rv) {
                    const char *rc = "int";
                    if (rt) {
                        if (!strcmp(rt, "string")) rc = "char *";
                        else if (!strcmp(rt, "json_object")) rc = "cJSON *";
                        else if (!strcmp(rt, "subst_table")) rc = "subst_table *";
                        else if (!strcmp(rt, "int")) rc = "int";
                    }
                    fprintf(out, "%s %s = ", rc, rv);
                }
                fprintf(out, "%s(", fn);
                if (args && cJSON_IsObject(args)) {
                    cJSON *arg = args->child; int first = 1;
                    while (arg) {
                        if (!first) fprintf(out, ", ");
                        if (cJSON_IsString(arg)) {
                            const char *sv = arg->valuestring;
                            if (sv[0] == '"') fprintf(out, "%s", sv);
                            else fprintf(out, "%s", sv);
                        } else if (cJSON_IsNumber(arg)) fprintf(out, "\"%d\"", arg->valueint);
                        first = 0; arg = arg->next;
                    }
                }
                fprintf(out, ");\n");
            }
        } else if (!strcmp(type, "conditional_branch")) {
            const char *op = builtin_jstr(inst, "condition_operation");
            const char *tgt = "";
            cJSON *ct = cJSON_GetObjectItemCaseSensitive(inst, "condition_target");
            if (cJSON_IsString(ct)) tgt = ct->valuestring;
            else if (ct) { cJSON *s2 = cJSON_GetObjectItemCaseSensitive(ct, "source"); if (cJSON_IsString(s2)) tgt = s2->valuestring; }
            const char *cv = builtin_jstr(inst, "condition_value");
            if (!strcmp(op, "key_exists")) {
                fprintf(out, "if (cJSON_HasObjectItem(%s, \"%s\")) {\n", tgt, builtin_jstr(inst, "condition_key"));
            } else if (!strcmp(op, "is_not_null")) {
                fprintf(out, "if (%s != NULL) {\n", tgt);
            } else {
                fprintf(out, "if (strcmp(%s, \"%s\") == 0) {\n", tgt, cv ? cv : "");
            }
            cJSON *bon = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_success");
            builtin_compile_instructions(bon, out, indent + 1, return_type);
            fprintf(out, "%*c} else {\n", indent * 2, ' ');
            cJSON *bof = cJSON_GetObjectItemCaseSensitive(inst, "branch_on_failure");
            builtin_compile_instructions(bof, out, indent + 1, return_type);
            fprintf(out, "%*c}\n", indent * 2, ' ');
        } else if (!strcmp(type, "return_statement")) {
            cJSON *rp = cJSON_GetObjectItemCaseSensitive(inst, "return_payload");
            int is_void = return_type && !strcmp(return_type, "void");
            if (rp) {
                const char *es = builtin_jstr(rp, "execution_status");
                const char *rv = builtin_jstr(rp, "value");
                if (es && !strcmp(es, "failure"))
                    fprintf(out, "die(\"%s\");%s\n", builtin_jstr(rp, "error_code"), is_void ? "" : " return 1;");
                else if (rv)
                    fprintf(out, "return %s;\n", rv);
                else if (is_void)
                    fprintf(out, "return;\n");
                else
                    fprintf(out, "return 0;\n");
            } else {
                fprintf(out, "%s\n", is_void ? "return;" : "return 0;");
            }
        } else if (!strcmp(type, "iterate_over_object_keys")) {
            const char *col = builtin_jstr(inst, "collection_variable");
            const char *key = builtin_jstr(inst, "key_variable");
            const char *val = builtin_jstr(inst, "value_variable");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
            if (col && val && body) {
                fprintf(out, "cJSON *%s = %s;\n", val, col);
                fprintf(out, "cJSON_ArrayForEach(%s, %s) {\n", val, col);
                if (key) fprintf(out, "  const char *%s = %s->string;\n", key, val);
                fprintf(out, "  const char *%s_valstr = %s->valuestring;\n", val, val);
                builtin_compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "}\n");
            }
        } else if (!strcmp(type, "iterate_over_collection")) {
            const char *col = builtin_jstr(inst, "collection_variable");
            const char *item = builtin_jstr(inst, "item_variable");
            cJSON *body = cJSON_GetObjectItemCaseSensitive(inst, "body_instructions");
            if (col && item && body) {
                fprintf(out, "for (int _i_%s = 0; _i_%s < cJSON_GetArraySize(%s); _i_%s++) {\n", col, col, col, col);
                fprintf(out, "  cJSON *%s = cJSON_GetArrayItem(%s, _i_%s);\n", item, col, col);
                builtin_compile_instructions(body, out, indent + 1, return_type);
                fprintf(out, "%*c}\n", indent * 2, ' ');
            }
        } else if (!strcmp(type, "variable_declaration")) {
            const char *vn = builtin_jstr(inst, "variable_name");
            const char *vt = builtin_jstr(inst, "variable_type");
            const char *op = builtin_jstr(inst, "assignment_operation");
            cJSON *st = cJSON_GetObjectItemCaseSensitive(inst, "source_target");
            const char *src = "";
            if (cJSON_IsString(st)) src = st->valuestring;
            if (vn && op) fprintf(out, "%s %s = %s(%s);\n", builtin_vartype_to_c(vt), vn, op, src);
        }
    }
}

void compile_ast_functions_to_c(cJSON *spec_json, const char *output_path) {
    cJSON *funcs = cJSON_GetObjectItemCaseSensitive(spec_json, "function_definitions");
    FILE *out = output_path ? fopen(output_path, "a") : stdout;
    if (!out) return;
    if (!funcs || !cJSON_IsObject(funcs)) {
        fprintf(out, "/* compile_ast: no function_definitions */\n");
        if (output_path) fclose(out);
        return;
    }

    /* Emit includes */
    fprintf(out, "#include <string.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <cjson/cJSON.h>\n#include \"ipm_builtins.h\"\n\n");
    cJSON *fn = funcs->child;
    /* forward declarations */
    while (fn) {
        const char *name = fn->string;
        const char *ret = builtin_jstr(fn, "return_type");
        const char *ret_c = "void";
        if (ret) {
            if (!strcmp(ret, "void")) ret_c = "void";
            else if (!strcmp(ret, "int")) ret_c = "int";
            else if (!strcmp(ret, "string")) ret_c = "char *";
            else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
            else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
        }
        fprintf(out, "static %s %s(", ret_c, name);
        cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
        if (params && cJSON_IsArray(params)) {
            for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                cJSON *par = cJSON_GetArrayItem(params, p);
                const char *pn = builtin_jstr(par, "parameter_name");
                const char *pt = builtin_jstr(par, "parameter_type");
                if (p > 0) fprintf(out, ", ");
                fprintf(out, "%s %s", builtin_vartype_to_c(pt), pn);
            }
        }
        fprintf(out, ");\n");
        fn = fn->next;
    }
    fprintf(out, "\n");
    /* function bodies */
    fn = funcs->child;
    while (fn) {
        const char *name = fn->string;
        const char *desc = builtin_jstr(fn, "description");
        const char *ret = builtin_jstr(fn, "return_type");
        const char *ret_c = "void";
        if (ret) {
            if (!strcmp(ret, "void")) ret_c = "void";
            else if (!strcmp(ret, "int")) ret_c = "int";
            else if (!strcmp(ret, "string")) ret_c = "char *";
            else if (!strcmp(ret, "json_object")) ret_c = "cJSON *";
            else if (!strcmp(ret, "subst_table")) ret_c = "subst_table *";
        }
        fprintf(out, "/* %s: %s */\n", name, desc ? desc : "no description");
        fprintf(out, "static %s %s(", ret_c, name);
        cJSON *params = cJSON_GetObjectItemCaseSensitive(fn, "parameter_definitions");
        if (params && cJSON_IsArray(params)) {
            for (int p = 0; p < cJSON_GetArraySize(params); p++) {
                cJSON *par = cJSON_GetArrayItem(params, p);
                const char *pn = builtin_jstr(par, "parameter_name");
                const char *pt = builtin_jstr(par, "parameter_type");
                if (p > 0) fprintf(out, ", ");
                fprintf(out, "%s %s", builtin_vartype_to_c(pt), pn);
            }
        }
        fprintf(out, ") {\n");
        cJSON *body = cJSON_GetObjectItemCaseSensitive(fn, "execution_instructions");
        builtin_compile_instructions(body, out, 1, ret_c);
        fprintf(out, "}\n\n");
        fn = fn->next;
    }
    if (output_path) fclose(out);
}
