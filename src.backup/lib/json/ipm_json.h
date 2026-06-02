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
json_node *json_parse(const char *text);

/* cjson-compatible builder API */
typedef json_node cJSON;
#define cJSON_IsString(n)  ((n) && (n)->type == JSTR)
#define cJSON_IsNumber(n)  ((n) && (n)->type == JINT)
#define cJSON_IsArray(n)   ((n) && (n)->type == JARR)
#define cJSON_IsObject(n)  ((n) && (n)->type == JOBJ)
#define cJSON_IsBool(n)    ((n) && (n)->type == JBOOL)
#define cJSON_IsTrue(n)    ((n) && (n)->type == JBOOL && (n)->ival)
#define cJSON_IsFalse(n)   ((n) && (n)->type == JBOOL && !(n)->ival)

cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateString(const char *s);
cJSON *cJSON_CreateNumber(double v);

cJSON *cJSON_AddStringToObject(cJSON *obj, const char *key, const char *val);
cJSON *cJSON_AddNumberToObject(cJSON *obj, const char *key, double num);
cJSON *cJSON_AddBoolToObject(cJSON *obj, const char *key, int b);
cJSON *cJSON_AddItemToObject(cJSON *obj, const char *key, cJSON *item);
void   cJSON_AddItemToArray(cJSON *arr, cJSON *item);

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *obj, const char *key);
int    cJSON_GetArraySize(const cJSON *arr);
cJSON *cJSON_GetArrayItem(const cJSON *arr, int idx);
int    cJSON_HasObjectItem(const cJSON *obj, const char *key);

cJSON *cJSON_Parse(const char *text);
char  *cJSON_PrintUnformatted(const cJSON *n);
char  *cJSON_Print(const cJSON *n);
void   cJSON_Delete(cJSON *n);
cJSON *cJSON_Duplicate(const cJSON *n, int recurse);
void   cJSON_SetNumberValue(cJSON *n, double v);
void   cJSON_SetValuestring(cJSON *n, const char *s);
cJSON *cJSON_CreateBool(int b);

#define cJSON_ArrayForEach(element, array) for(element = (array) ? (array)->arr ? (array)->arr[0] : NULL : NULL; element != NULL; element = element->next)
#define cJSON_CreateIntArray(arr,count) cJSON_CreateArray()
#endif
