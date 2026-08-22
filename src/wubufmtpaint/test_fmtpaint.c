/* test_fmtpaint.c -- format painter over real wubumodel styles. */
#include "fmtpaint.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *src = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_node *dst = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_style *s = wubumodel_style_create();
    wubumodel_style_set_prop(s, "bold", "1");
    wubumodel_style_set_prop(s, "italic", "1");
    wubumodel_style_set_prop(s, "color", "#ff0000");
    wubumodel_node_set_style(src, s);

    FmtPaint *f = fmtpaint_create();
    CK(fmtpaint_loaded(f)==0, "empty brush");
    int picked = fmtpaint_pick(f, src);
    CK(picked==3, "picked 3 props");
    CK(fmtpaint_value(f,"bold") && strcmp(fmtpaint_value(f,"bold"),"1")==0, "brush bold");
    CK(fmtpaint_value(f,"color") && strcmp(fmtpaint_value(f,"color"),"#ff0000")==0, "brush color");

    int applied = fmtpaint_apply(f, dst);
    CK(applied==3, "applied 3 props");
    wubumodel_style *ds = wubumodel_node_style(dst);
    CK(ds != NULL, "dst has style");
    CK(ds && strcmp(wubumodel_style_get_prop(ds,"bold"),"1")==0, "dst bold");
    CK(ds && strcmp(wubumodel_style_get_prop(ds,"italic"),"1")==0, "dst italic");
    CK(ds && strcmp(wubumodel_style_get_prop(ds,"color"),"#ff0000")==0, "dst color");
    /* fresh style, not aliased: mutate dst, src unchanged */
    wubumodel_style_set_prop(ds, "bold", "0");
    CK(strcmp(wubumodel_style_get_prop(wubumodel_node_style(src),"bold"),"1")==0,
       "src not aliased");
    /* pick from style-less node clears brush */
    wubumodel_node *bare = wubumodel_node_create(d, WUBUMODEL_RUN);
    CK(fmtpaint_pick(f, bare)==0, "bare pick 0");
    CK(fmtpaint_loaded(f)==0, "brush cleared");
    fmtpaint_destroy(f);
    /* drop creator refs: node_set_style took a ref (refcount 2) */
    wubumodel_style_destroy(s);
    wubumodel_doc_destroy(d);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: fmtpaint (pick/apply/no-alias)\n"); return 0;
}
