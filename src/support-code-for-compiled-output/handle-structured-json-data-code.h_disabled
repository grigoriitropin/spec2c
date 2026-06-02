// SPDX-License-Identifier: Apache-2.0
// ipm_json.h — constrained JSON reader + cjson-compatible builder
#ifndef IPM_CJSON_COMPAT_H
#define IPM_CJSON_COMPAT_H
#include <stdint.h>
#include <stddef.h>

typedef enum { JNULL, JBOOL, JINT, JSTR, JARR, JOBJ } json_type;

typedef struct json_node {
    json_type type;
    union { int64_t ival; char *sval; struct json_node **arr;
            struct { char *key; struct json_node *val; } *pairs; };
} json_node;

/* Parser (spec2c-generated JSON only) */
json_node *parse_json_text_into_object(const char *text);

/* cjson-compatible builder API */
typedef json_node cJSON;
#define cJSON_IsString(n)  ((n) && (n)->type == JSTR)
#define cJSON_IsNumber(n)  ((n) && (n)->type == JINT)
#define cJSON_IsArray(n)   ((n) && (n)->type == JARR)
#define cJSON_IsObject(n)  ((n) && (n)->type == JOBJ)
#define cJSON_IsBool(n)    ((n) && (n)->type == JBOOL)
#define cJSON_IsTrue(n)    ((n) && (n)->type == JBOOL && (n)->ival)
#define cJSON_IsFalse(n)   ((n) && (n)->type == JBOOL && !(n)->ival)

cJSON *create_json_object_from_scratch(void);
cJSON *create_json_array_from_scratch(void);
cJSON *create_json_string_from_scratch(const char *s);
cJSON *create_json_number_from_scratch(double v);

cJSON *add_json_string_to_parent_object(cJSON *obj, const char *key, const char *val);
cJSON *add_json_number_to_parent_object(cJSON *obj, const char *key, double num);
cJSON *add_json_boolean_to_parent_object(cJSON *obj, const char *key, int b);
cJSON *add_json_item_to_parent_object(cJSON *obj, const char *key, cJSON *item);
void   add_json_item_to_parent_array(cJSON *arr, cJSON *item);

cJSON *find_object_item_case_sensitive(const cJSON *obj, const char *key);
int    get_array_size_from_json(const cJSON *arr);
cJSON *get_array_item_from_json(const cJSON *arr, int idx);
int    check_object_has_json_item(const cJSON *obj, const char *key);

cJSON *parse_raw_text_into_json(const char *text);
char  *print_json_without_formatting(const cJSON *n);
char  *print_json_with_formatting(const cJSON *n);
void   delete_json_object_from_memory(cJSON *n);
cJSON *duplicate_json_object_deep_copy(const cJSON *n, int recurse);
void   set_json_number_field_value(cJSON *n, double v);
void   set_json_valu_string_field(cJSON *n, const char *s);
cJSON *create_json_boolean_from_scratch(int b);

#define cJSON_ArrayForEach(element, array) for(element = (array) ? (array)->arr ? (array)->arr[0] : NULL : NULL; element != NULL; element = element->next)
#define cJSON_CreateIntArray(arr,count) create_json_array_from_scratch()
#endif
