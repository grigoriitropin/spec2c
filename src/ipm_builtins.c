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
    FILE *f = fopen(path, "w");
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
