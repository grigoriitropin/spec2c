// SPDX-License-Identifier: Apache-2.0
// ipm_json.h — constrained JSON reader (ASCII, int-only, depth ≤3, no user input)
#ifndef IPM_JSON_H
#define IPM_JSON_H
#include <stdint.h>
#include <stddef.h>

typedef enum { JNULL, JBOOL, JINT, JSTR, JARR, JOBJ } json_type;

typedef struct json_node {
    json_type type;
    union {
        int64_t ival;
        char *sval;
        struct json_node **arr;
        struct { char *key; struct json_node *val; } *pairs;
    };
} json_node;

json_node *json_parse(const char *text);
char      *json_print(const json_node *n);
void       json_free(json_node *n);

#endif
