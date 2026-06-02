// SPDX-License-Identifier: Apache-2.0
// ipm_json.c — constrained JSON parser + cjson-compatible builder
#include "handle-structured-json-data-code.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── parser (from earlier) ──────────────────────────────────────────── */
static const char *skip_ws(const char *p) { while(*p==' '||*p=='\n'||*p=='\t'||*p=='\r') p++; return p; }
static json_node *parse_json_value_into_buffer(const char **pp, int depth);

static json_node *parse_json_string_into_buffer(const char **pp) {
    const char *p = *pp + 1; char buf[4096]; int i=0;
    while (*p && *p != '"' && i < 4095) { if (*p == '\\') { p++; if (*p) buf[i++] = *p++; } else buf[i++] = *p++; }
    buf[i]=0; if (*p=='"') p++; *pp=p;
    json_node *n=calloc(1,sizeof(json_node)); n->type=JSTR; n->sval=strdup(buf); return n;
}
static json_node *parse_json_array_into_buffer(const char **pp, int depth) {
    const char *p=*pp+1; json_node *n=calloc(1,sizeof(json_node)); n->type=JARR;
    json_node *items[256]; int cnt=0; p=skip_ws(p);
    while(*p&&*p!=']'&&cnt<255){if(cnt){if(*p!=',')break;p=skip_ws(p+1);}items[cnt++]=parse_json_value_into_buffer(&p,depth+1);p=skip_ws(p);}
    if(*p==']')p++; items[cnt]=NULL; n->arr=calloc(cnt+1,sizeof(json_node*)); memcpy(n->arr,items,cnt*sizeof(json_node*)); *pp=p; return n;
}
static json_node *parse_json_object_into_buffer(const char **pp, int depth) {
    if(depth>3)return NULL; const char *p=*pp+1; json_node *n=calloc(1,sizeof(json_node)); n->type=JOBJ;
    char *keys[64]; json_node *vals[64]; int cnt=0; p=skip_ws(p);
    while(*p&&*p!='}'&&cnt<63){if(cnt){if(*p!=',')break;p=skip_ws(p+1);}if(*p!='"')break;json_node*k=parse_json_string_into_buffer(&p);keys[cnt]=k->sval;free(k);p=skip_ws(p);if(*p!=':')break;p=skip_ws(p+1);vals[cnt++]=parse_json_value_into_buffer(&p,depth+1);p=skip_ws(p);}
    if(*p=='}')p++; keys[cnt]=NULL; n->pairs=calloc(cnt+1,sizeof(*n->pairs));
    for(int i=0;i<cnt;i++){n->pairs[i].key=keys[i];n->pairs[i].val=vals[i];} *pp=p; return n;
}
static json_node *parse_json_value_into_buffer(const char **pp, int depth) {
    const char *p=skip_ws(*pp); if(!*p)return NULL;
    if(*p=='"'){json_node*n=parse_json_string_into_buffer(&p);*pp=p;return n;}
    if(*p=='['){json_node*n=parse_json_array_into_buffer(&p,depth);*pp=p;return n;}
    if(*p=='{'){json_node*n=parse_json_object_into_buffer(&p,depth);*pp=p;return n;}
    if(!strncmp(p,"null",4)){json_node*n=calloc(1,sizeof(json_node));n->type=JNULL;*pp=p+4;return n;}
    if(!strncmp(p,"true",4)||!strncmp(p,"false",5)){json_node*n=calloc(1,sizeof(json_node));n->type=JBOOL;n->ival=*p=='t'?1:0;*pp=p+(*p=='t'?4:5);return n;}
    int sign=1;if(*p=='-'){sign=-1;p++;} int64_t v=0;while(*p>='0'&&*p<='9'){v=v*10+(*p-'0');p++;}
    json_node*n=calloc(1,sizeof(json_node));n->type=JINT;n->ival=v*sign;*pp=p;return n;
}
json_node *parse_json_text_into_object(const char *text) { const char *p=skip_ws(text); return parse_json_value_into_buffer(&p,0); }

/* ── cjson-compatible builder ───────────────────────────────────────── */

cJSON *create_json_object_from_scratch(void) { cJSON *n=calloc(1,sizeof(cJSON)); n->type=JOBJ; return n; }
cJSON *create_json_array_from_scratch(void)  { cJSON *n=calloc(1,sizeof(cJSON)); n->type=JARR; return n; }
cJSON *create_json_string_from_scratch(const char *s) { cJSON *n=calloc(1,sizeof(cJSON)); n->type=JSTR; n->sval=s?strdup(s):NULL; return n; }
cJSON *create_json_number_from_scratch(double v) { cJSON *n=calloc(1,sizeof(cJSON)); n->type=JINT; n->ival=(int64_t)v; return n; }
cJSON *create_json_boolean_from_scratch(int b) { cJSON *n=calloc(1,sizeof(cJSON)); n->type=JBOOL; n->ival=b?1:0; return n; }

static void append_child_to_json_object(cJSON *obj, const char *key, cJSON *val) {
    if(!obj||obj->type!=JOBJ||!key)return;
    int cnt=0; while(obj->pairs && obj->pairs[cnt].key) cnt++;
    obj->pairs = realloc(obj->pairs, (cnt+2)*sizeof(*obj->pairs));
    obj->pairs[cnt].key = strdup(key);
    obj->pairs[cnt].val = val;
    obj->pairs[cnt+1].key = NULL;
    obj->pairs[cnt+1].val = NULL;
}

cJSON *add_json_string_to_parent_object(cJSON *obj, const char *key, const char *val) { cJSON *s=create_json_string_from_scratch(val); append_child_to_json_object(obj,key,s); return s; }
cJSON *add_json_number_to_parent_object(cJSON *obj, const char *key, double num) { cJSON *n=create_json_number_from_scratch(num); append_child_to_json_object(obj,key,n); return n; }
cJSON *add_json_boolean_to_parent_object(cJSON *obj, const char *key, int b) { cJSON *n=create_json_boolean_from_scratch(b); append_child_to_json_object(obj,key,n); return n; }
cJSON *add_json_item_to_parent_object(cJSON *obj, const char *key, cJSON *item) { append_child_to_json_object(obj,key,item); return item; }

void add_json_item_to_parent_array(cJSON *arr, cJSON *item) {
    if(!arr||arr->type!=JARR||!item)return;
    int cnt=0; while(arr->arr && arr->arr[cnt]) cnt++;
    arr->arr = realloc(arr->arr, (cnt+2)*sizeof(cJSON*));
    arr->arr[cnt]=item; arr->arr[cnt+1]=NULL;
}

cJSON *find_object_item_case_sensitive(const cJSON *obj, const char *key) {
    if(!obj||obj->type!=JOBJ||!obj->pairs||!key) return NULL;
    for(int i=0; obj->pairs[i].key; i++) if(!strcmp(obj->pairs[i].key,key)) return obj->pairs[i].val;
    return NULL;
}

int get_array_size_from_json(const cJSON *arr) { if(!arr||arr->type!=JARR||!arr->arr)return 0; int n=0;while(arr->arr[n])n++;return n; }
cJSON *get_array_item_from_json(const cJSON *arr, int idx) { if(!arr||arr->type!=JARR||!arr->arr)return NULL; return arr->arr[idx]; }
int check_object_has_json_item(const cJSON *obj, const char *key) { return find_object_item_case_sensitive(obj,key) != NULL; }
cJSON *parse_raw_text_into_json(const char *text) { return parse_json_text_into_object(text); }

static void format_json_into_print_buffer(const cJSON *n, char **buf, size_t *sz, size_t *cap) {
    char tmp[64];
    switch(n->type){
    case JNULL: snprintf(tmp,64,"null"); break;
    case JBOOL: snprintf(tmp,64,"%s",n->ival?"true":"false"); break;
    case JINT:  snprintf(tmp,64,"%ld",(long)n->ival); break;
    case JSTR:  snprintf(tmp,64,"\"%s\"",n->sval?n->sval:""); break;
    case JOBJ:
        { size_t add=1;while(*sz+add+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);} (*buf)[(*sz)++]='{';
          for(int i=0;n->pairs&&n->pairs[i].key;i++){
            if(i){while(*sz+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}(*buf)[(*sz)++]=',';}
            const char *k=n->pairs[i].key; size_t kl=strlen(k);
            while(*sz+kl+4>*cap){*cap*=2;*buf=realloc(*buf,*cap);}
            (*buf)[(*sz)++]='"';memcpy(*buf+*sz,k,kl);*sz+=kl;(*buf)[(*sz)++]='"';(*buf)[(*sz)++]=':';
            format_json_into_print_buffer(n->pairs[i].val,buf,sz,cap);
          }
          while(*sz+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}(*buf)[(*sz)++]='}';}
        return;
    case JARR:
        { while(*sz+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}(*buf)[(*sz)++]='[';
          for(int i=0;n->arr&&n->arr[i];i++){ if(i){while(*sz+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}(*buf)[(*sz)++]=',';} format_json_into_print_buffer(n->arr[i],buf,sz,cap);}
          while(*sz+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}(*buf)[(*sz)++]=']';}
        return;
    }
    size_t nl=strlen(tmp);
    while(*sz+nl+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}
    memcpy(*buf+*sz,tmp,nl); *sz+=nl;
}

char *print_json_without_formatting(const cJSON *n) { char *b=malloc(256); size_t s=0,c=256; if(n)format_json_into_print_buffer(n,&b,&s,&c); b[s]=0; return b; }
char *print_json_with_formatting(const cJSON *n) { return print_json_without_formatting(n); }

void delete_json_object_from_memory(cJSON *n) {
    if(!n)return;
    if(n->type==JSTR) free(n->sval);
    if(n->type==JARR){ for(int i=0;n->arr&&n->arr[i];i++) cJSON_Delete(n->arr[i]); free(n->arr); }
    if(n->type==JOBJ){ for(int i=0;n->pairs&&n->pairs[i].key;i++){free(n->pairs[i].key);cJSON_Delete(n->pairs[i].val);} free(n->pairs); }
    free(n);
}

cJSON *duplicate_json_object_deep_copy(const cJSON *n, int recurse) { (void)recurse; if(!n)return NULL; char *s=print_json_without_formatting(n); cJSON *d=parse_raw_text_into_json(s); free(s); return d; }
void set_json_number_field_value(cJSON *n, double v) { if(n){n->type=JINT; n->ival=(int64_t)v;} }
void set_json_valu_string_field(cJSON *n, const char *s) { if(n){n->sval = s ? strdup(s) : NULL;} }
