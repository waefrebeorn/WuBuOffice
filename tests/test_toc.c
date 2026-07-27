/* test_toc.c -- DOC-54 TOC generator + UXA-41 high-contrast settings. */
#include "toc.h"
#include "model.h"
#include "ublayout.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

/* build a doc with a section, three headings (h1/h2/h3) and two paras,
 * then lay it out and verify the TOC resolves page numbers from the layout. */
static wubumodel_node *add_para(wubumodel_doc *doc, wubumodel_node *parent,
                                 const char *txt, const char *heading){
    wubumodel_node *p = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    if (heading){
        wubumodel_style *s = wubumodel_style_create();
        char buf[4]; snprintf(buf,sizeof buf,"%s",heading);
        wubumodel_style_set_prop(s, "heading", buf);
        wubumodel_node_set_style(p, s);
        wubumodel_style_destroy(s);
    }
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, txt);
    wubumodel_node_append(doc, p, r);
    wubumodel_node_append(doc, parent, p);
    return p;
}

static int meas(const char *t, size_t len, int fs, int b, int i, int *h, void *u){
    (void)b;(void)i;(void)u;(void)fs;
    int w=0; for(size_t k=0;k<len;k++) w += (t[k]==' '?4:7); *h=12+4; return w;
}
static int sty(void *u, void *run, int *fs,int *b,int *it,wubulayout_dir *d){
    (void)u;(void)run;(void)b;(void)it;(void)d; *fs=12; *b=0; *it=0; *d=WUBULAYOUT_LTR; return 1;
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    add_para(doc, sec, "Introduction", "1");
    add_para(doc, sec, "lorem ipsum dolor sit amet consectetur adipiscing elit", NULL);
    add_para(doc, sec, "Background", "2");
    add_para(doc, sec, "sed do eiusmod tempor incididunt ut labore et dolore", NULL);
    add_para(doc, sec, "Methods", "3");
    add_para(doc, sec, "ut enim ad minim veniam quis nostrud exercitation", NULL);

    /* build TOC WITHOUT layout (pages must be 0, but titles/levels present) */
    Toc *t0 = toc_build(doc, sec, NULL);
    CK(t0 != NULL, "toc built without layout");
    CK(toc_count(t0) == 3, "three headings found");
    CK(strcmp(toc_title(t0,0), "Introduction")==0, "first heading title");
    CK(toc_level(t0,0) == 1, "first heading level 1");
    CK(toc_level(t0,1) == 2, "second heading level 2");
    CK(toc_page(t0,0) == 0, "no-layout -> page 0");
    free(toc_text(t0));
    toc_free(t0);

    /* now lay it out (tiny page forces multi-page) and re-build with layout */
    wubulayout_doc *L = wubulayout_create(doc, sec, meas, sty, NULL, 200, 200, 10,10,10,10);
    CK(L != NULL, "layout created");
    Toc *t = toc_build(doc, sec, L);
    CK(t != NULL, "toc built with layout");
    CK(toc_count(t) == 3, "three headings (with layout)");
    /* every heading must resolve to a real 1-based page within the doc */
    int pages = wubulayout_page_count(L);
    for (int i=0;i<toc_count(t);i++){
        int pg = toc_page(t,i);
        CK(pg >= 1 && pg <= pages, "heading resolves to a valid page");
    }
    /* TOC text form: indented + "pN" */
    char *txt = toc_text(t);
    CK(txt && strstr(txt, "Introduction") && strstr(txt, "p"), "toc_text indented with page");
    free(txt);
    toc_free(t);
    wubulayout_destroy(L);

    /* UXA-41: high-contrast setting persists + toggles */
    WubuSettings *s = wubusettings_create();
    CK(wubusettings_high_contrast(s)==0, "hc off by default");
    wubusettings_set_high_contrast(s, 1);
    CK(wubusettings_high_contrast(s)==1, "hc turns on");
    wubusettings_save(s, "/tmp/wubuos_hc.json");
    wubusettings_destroy(s);
    s = wubusettings_create();
    CK(wubusettings_load(s, "/tmp/wubuos_hc.json")==0, "hc settings load");
    CK(wubusettings_high_contrast(s)==1, "hc persisted across save/load");
    wubusettings_destroy(s);

    wubumodel_doc_destroy(doc);
    if (fails==0) printf("TOC + SETTINGS TESTS PASSED\n");
    else printf("TOC + SETTINGS TESTS FAILED (%d)\n", fails);
    return fails?1:0;
}
