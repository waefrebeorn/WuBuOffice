/* fmtpaint.c -- format painter. See fmtpaint.h. */
#include "fmtpaint.h"
#include "model.h"

#include <stdlib.h>
#include <string.h>

#define FP_MAX 64

typedef struct { char *name; char *value; } Prop;

struct FmtPaint { Prop p[FP_MAX]; int n; };

FmtPaint *fmtpaint_create(void){ return calloc(1, sizeof(FmtPaint)); }

void fmtpaint_clear(FmtPaint *f){
    if (!f) return;
    for (int i=0;i<f->n;i++){ free(f->p[i].name); free(f->p[i].value); }
    f->n = 0;
}
void fmtpaint_destroy(FmtPaint *f){ fmtpaint_clear(f); free(f); }

int fmtpaint_pick(FmtPaint *f, const void *src){
    if (!f || !src) return 0;
    fmtpaint_clear(f);
    const wubumodel_node *n = (const wubumodel_node*)src;
    wubumodel_style *s = wubumodel_node_style(n);
    if (!s) return 0;
    const char *name, *value;
    for (int i=0; f->n<FP_MAX && wubumodel_style_prop_at(s, i, &name, &value); i++){
        f->p[f->n].name  = strdup(name);
        f->p[f->n].value = value ? strdup(value) : NULL;
        f->n++;
    }
    return f->n;
}

int fmtpaint_apply(FmtPaint *f, void *dst){
    if (!f || !dst) return -1;
    if (f->n == 0) return 0;
    wubumodel_style *s = wubumodel_style_create();
    if (!s) return -1;
    for (int i=0;i<f->n;i++)
        wubumodel_style_set_prop(s, f->p[i].name, f->p[i].value);
    if (wubumodel_node_set_style((wubumodel_node*)dst, s) != 0){
        wubumodel_style_destroy(s);
        return -1;
    }
    /* set_style took a refcount; release ours */
    wubumodel_style_destroy(s);
    return f->n;
}

int fmtpaint_loaded(const FmtPaint *f){ return f ? f->n : 0; }
const char *fmtpaint_value(const FmtPaint *f, const char *name){
    if (!f || !name) return NULL;
    for (int i=0;i<f->n;i++)
        if (strcmp(f->p[i].name, name)==0) return f->p[i].value;
    return NULL;
}
