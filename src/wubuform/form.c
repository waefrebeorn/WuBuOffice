/* form.c -- PDF form-field data model. See form.h. */
#include "form.h"

#include <stdlib.h>
#include <string.h>

#define FORM_MAX 256
typedef struct { char name[64]; FormType type; char value[256]; } F;

struct Form { F e[FORM_MAX]; int n; };

Form *form_create(void){ return calloc(1, sizeof(Form)); }
void form_destroy(Form *f){ free(f); }

static F *find(Form *f, const char *name){
    for (int i=0;i<f->n;i++) if (!strcmp(f->e[i].name, name)) return &f->e[i];
    return NULL;
}

int form_add(Form *f, const char *name, FormType type, const char *value){
    if (!f || !name || f->n>=FORM_MAX) return 0;
    if (find(f, name)) return 0;
    strncpy(f->e[f->n].name, name, 63); f->e[f->n].name[63]=0;
    f->e[f->n].type = type;
    strncpy(f->e[f->n].value, value?value:"", 255); f->e[f->n].value[255]=0;
    f->n++;
    return 1;
}
int form_set_value(Form *f, const char *name, const char *value){
    F *e = find(f, name); if (!e) return 0;
    strncpy(e->value, value?value:"", 255); e->value[255]=0; return 1;
}
const char *form_value(const Form *f, const char *name){ F *e=find((Form*)f,name); return e? e->value : NULL; }
FormType form_type(const Form *f, const char *name){ F *e=find((Form*)f,name); return e? e->type : FORM_TEXT; }
int form_count(const Form *f){ return f? f->n : 0; }
const char *form_name_at(const Form *f, int i){ return (f&&i>=0&&i<f->n)? f->e[i].name : NULL; }
