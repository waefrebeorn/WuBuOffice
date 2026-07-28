/* vars.c -- named document variables. See vars.h. */
#include "vars.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define VARS_MAX 256

typedef struct { char *name; char *value; } Entry;

struct Vars {
    Entry e[VARS_MAX];
    int n;
};

Vars *vars_create(void){ return calloc(1, sizeof(Vars)); }

void vars_destroy(Vars *v){
    if (!v) return;
    for (int i=0;i<v->n;i++){ free(v->e[i].name); free(v->e[i].value); }
    free(v);
}

int vars_set(Vars *v, const char *name, const char *value){
    if (!v || !name) return 0;
    for (int i=0;i<v->n;i++)
        if (!strcmp(v->e[i].name, name)){
            free(v->e[i].value); v->e[i].value = value? strdup(value):strdup("");
            return 1;
        }
    if (v->n >= VARS_MAX) return 0;
    v->e[v->n].name = strdup(name);
    v->e[v->n].value = value? strdup(value):strdup("");
    v->n++;
    return 1;
}

const char *vars_get(const Vars *v, const char *name){
    if (!v || !name) return NULL;
    for (int i=0;i<v->n;i++) if (!strcmp(v->e[i].name, name)) return v->e[i].value;
    return NULL;
}

char *vars_expand(const Vars *v, const char *text){
    if (!text) return NULL;
    size_t cap = strlen(text)+1, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    const char *p = text;
    while (*p){
        if (*p == '$' && *(p+1) == '{'){
            const char *end = strchr(p+2, '}');
            if (end){
                char key[128]; size_t kl = (size_t)(end-(p+2));
                if (kl >= sizeof key) kl = sizeof key -1;
                memcpy(key, p+2, kl); key[kl]=0;
                const char *val = vars_get(v, key);
                size_t vl = val? strlen(val):0;
                while (len+vl+1 > cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
                if (val){ memcpy(out+len, val, vl); len+=vl; }
                else { memcpy(out+len, p, (size_t)(end-p+1)); len += (size_t)(end-p+1); }
                p = end+1;
                continue;
            }
        }
        while (len+2 > cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
        out[len++] = *p++;
    }
    out[len]=0;
    return out;
}

int vars_count(const Vars *v){ return v? v->n : 0; }
const char *vars_name_at(const Vars *v, int i){ return (v&&i>=0&&i<v->n)? v->e[i].name : NULL; }
