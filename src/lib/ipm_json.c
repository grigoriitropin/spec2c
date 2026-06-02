// SPDX-License-Identifier: Apache-2.0
// ipm_json.c — constrained JSON reader. Only for spec2c-generated JSON.
#include "ipm_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *skip_ws(const char *p) { while(*p==' '||*p=='\n'||*p=='\t'||*p=='\r') p++; return p; }

static json_node *parse_val(const char **pp, int depth);

static json_node *parse_str(const char **pp) {
    const char *p = *pp + 1; /* skip opening " */
    char buf[4096]; int i=0;
    while (*p && *p != '"' && i < 4095) {
        if (*p == '\\') { p++; if (*p) buf[i++] = *p++; }
        else buf[i++] = *p++;
    }
    buf[i] = 0;
    if (*p == '"') p++;
    *pp = p;
    json_node *n = calloc(1,sizeof(json_node));
    n->type = JSTR; n->sval = strdup(buf);
    return n;
}

static json_node *parse_arr(const char **pp, int depth) {
    const char *p = *pp + 1;
    json_node *n = calloc(1,sizeof(json_node));
    n->type = JARR;
    json_node *items[256]; int cnt = 0;
    p = skip_ws(p);
    while (*p && *p != ']' && cnt < 255) {
        if (cnt) { if (*p != ',') break; p = skip_ws(p+1); }
        items[cnt++] = parse_val(&p, depth+1);
        p = skip_ws(p);
    }
    if (*p == ']') p++;
    items[cnt] = NULL;
    n->arr = calloc(cnt+1, sizeof(json_node*));
    memcpy(n->arr, items, cnt*sizeof(json_node*));
    *pp = p;
    return n;
}

static json_node *parse_obj(const char **pp, int depth) {
    if (depth > 3) return NULL;
    const char *p = *pp + 1;
    json_node *n = calloc(1,sizeof(json_node));
    n->type = JOBJ;
    char *keys[64]; json_node *vals[64]; int cnt = 0;
    p = skip_ws(p);
    while (*p && *p != '}' && cnt < 63) {
        if (cnt) { if (*p != ',') break; p = skip_ws(p+1); }
        if (*p != '"') break;
        json_node *k = parse_str(&p);
        keys[cnt] = k->sval; free(k);
        p = skip_ws(p);
        if (*p != ':') break;
        p = skip_ws(p+1);
        vals[cnt++] = parse_val(&p, depth+1);
        p = skip_ws(p);
    }
    if (*p == '}') p++;
    keys[cnt] = NULL;
    n->pairs = calloc(cnt+1, sizeof(*n->pairs));
    for (int i=0;i<cnt;i++){ n->pairs[i].key = keys[i]; n->pairs[i].val = vals[i]; }
    *pp = p;
    return n;
}

static json_node *parse_val(const char **pp, int depth) {
    const char *p = skip_ws(*pp);
    if (!*p) return NULL;
    json_node *n;
    if (*p == '"') { n = parse_str(&p); *pp = p; return n; }
    if (*p == '[') { n = parse_arr(&p, depth); *pp = p; return n; }
    if (*p == '{') { n = parse_obj(&p, depth); *pp = p; return n; }
    if (!strncmp(p,"null",4)) { n=calloc(1,sizeof(json_node)); n->type=JNULL; *pp=p+4; return n; }
    if (!strncmp(p,"true",4)||!strncmp(p,"false",5)) {
        n=calloc(1,sizeof(json_node)); n->type=JBOOL;
        n->ival = *p=='t'?1:0; *pp = p+(*p=='t'?4:5); return n;
    }
    /* number */
    int sign=1; if(*p=='-'){sign=-1;p++;}
    int64_t v=0; while(*p>='0'&&*p<='9'){v=v*10+(*p-'0');p++;}
    n=calloc(1,sizeof(json_node)); n->type=JINT; n->ival=v*sign;
    *pp=p; return n;
}

json_node *json_parse(const char *text) {
    const char *p = skip_ws(text);
    json_node *n = parse_val(&p, 0);
    return n;
}

static void json_print_buf(const json_node *n, char **buf, size_t *sz, size_t *cap) {
    char tmp[64];
    switch(n->type){
    case JNULL: snprintf(tmp,64,"null"); break;
    case JBOOL: snprintf(tmp,64,"%s",n->ival?"true":"false"); break;
    case JINT:  snprintf(tmp,64,"%ld",(long)n->ival); break;
    case JSTR:  snprintf(tmp,64,"\"%s\"",n->sval); break;
    default: return;
    }
    size_t nl=strlen(tmp);
    while(*sz+nl+1>*cap){*cap*=2;*buf=realloc(*buf,*cap);}
    memcpy(*buf+*sz,tmp,nl); *sz+=nl;
}

char *json_print(const json_node *n) {
    if(!n) return strdup("null");
    char *buf=malloc(256); size_t sz=0,cap=256;
    json_print_buf(n,&buf,&sz,&cap);
    buf[sz]=0;
    return buf;
}

void json_free(json_node *n) {
    if(!n) return;
    if(n->type==JSTR) free(n->sval);
    if(n->type==JARR){ for(json_node**a=n->arr;a&&*a;a++) json_free(*a); free(n->arr); }
    if(n->type==JOBJ){ for(int i=0;n->pairs[i].key;i++){free(n->pairs[i].key);json_free(n->pairs[i].val);} free(n->pairs); }
    free(n);
}
