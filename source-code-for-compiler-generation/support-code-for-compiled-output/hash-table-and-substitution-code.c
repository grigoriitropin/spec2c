// SPDX-License-Identifier: Apache-2.0
// ipm_builtins — hash table, substitution, string buffer create/append (split part 2)
#include "../runtime-for-generated-ipm-code.h"

subst_table* allocate_and_init_hash_table(void) {
    subst_table *t = malloc(sizeof(subst_table));
    t->entries = malloc(16 * sizeof(subst_entry));
    t->count = 0;
    t->capacity = 16;
    return t;
}

static int compare_hash_table_key_entries(const void *a, const void *b) {
    return strcmp(((const subst_entry *)a)->key, ((const subst_entry *)b)->key);
}

void hash_table_insert_key_value(subst_table *table, const char *key, const char *value) {
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
    qsort(table->entries, (size_t)table->count, sizeof(subst_entry), compare_hash_table_key_entries);
}

char* hash_table_lookup_key_value(const subst_table *table, const char *key) {
    if (!table || !key) return NULL;
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].key, key) == 0)
            return table->entries[i].value;
    }
    return NULL;
}

void hash_table_free_all_entries(subst_table *table) {
    if (!table) return;
    for (int i = 0; i < table->count; i++) {
        free(table->entries[i].key);
        free(table->entries[i].value);
    }
    free(table->entries);
    free(table);
}

int hash_table_count_all_entries(const subst_table *table) {
    return table ? table->count : 0;
}

string apply_substitution_against_raw_text(const char *template_str, const subst_table *table) {
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
            const char *val = hash_table_lookup_key_value(table, kbuf);
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

string_buffer* create_empty_growable_string_buffer(void) {
    string_buffer *buf = malloc(sizeof(string_buffer));
    buf->cap = 8192;
    buf->data = malloc(buf->cap);
    buf->data[0] = '\0';
    buf->len = 0;
    return buf;
}

void append_text_into_string_buffer(string_buffer *buf, const char *str) {
    if (!buf || !str) return;
    size_t slen = strlen(str);
    while (buf->len + slen + 1 > buf->cap) {
        buf->cap *= 2;
        buf->data = realloc(buf->data, buf->cap);
    }
    memcpy(buf->data + buf->len, str, slen);
    buf->len += slen;
    buf->data[buf->len] = '\0';
}
