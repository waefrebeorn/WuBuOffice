/* test_exp.c -- exporters consume the layout (EXP cluster). */
#include "ublayout.h"
#include "exp.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

static int meas(const char *t, size_t len, int fs, int b, int i, int *h, void *u){
    (void)b;(void)i;(void)u; int w=0; for(size_t k=0;k<len;k++) w += (t[k]==' '? fs*0.3 : fs*0.55); *h=fs+4; return w;
}
static int sty(void *u, void *run, int *fs,int*b,int*it,wubulayout_dir*d){ (void)u;(void)run;(void)b;(void)it;(void)d; *fs=12;*b=0;*it=0;*d=WUBULAYOUT_LTR; return 1; }

static wubumodel_node *add_para(wubumodel_doc *doc, wubumodel_node *parent, const char *txt){
    wubumodel_node *p = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, txt); wubumodel_node_append(doc, p, r);
    wubumodel_node_append(doc, parent, p); return p;
}

static int file_nonempty(const char *p){
    FILE *f=fopen(p,"rb"); if(!f) return 0; fseek(f,0,SEEK_END); long n=ftell(f); fclose(f); return n>0;
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    add_para(doc, sec, "Hello & <world> from WuBuOffice export test.");
    add_para(doc, sec, "Second paragraph with $math$ and 100% done.");

    wubulayout_doc *L = wubulayout_create(doc, sec,
        meas, sty, NULL, 794,1123,72,72,72,72);
    CK(L!=NULL, "layout for export");

    CK(wubuexp_markdown(L, "/tmp/exp_test.md")==0 && file_nonempty("/tmp/exp_test.md"), "markdown export");
    CK(wubuexp_html(   L, "/tmp/exp_test.html")==0 && file_nonempty("/tmp/exp_test.html"), "html export");
    CK(wubuexp_latex(  L, "/tmp/exp_test.tex")==0 && file_nonempty("/tmp/exp_test.tex"), "latex export");
    CK(wubuexp_rtf(    L, "/tmp/exp_test.rtf")==0 && file_nonempty("/tmp/exp_test.rtf"), "rtf export");
    CK(wubuexp_pdf(    L, "/tmp/exp_test.pdf")==0 && file_nonempty("/tmp/exp_test.pdf"), "pdf export");

    /* PDF must start with %PDF */
    FILE *f=fopen("/tmp/exp_test.pdf","rb"); char hdr[8]; int n=fread(hdr,1,5,f); fclose(f);
    CK(n==5 && memcmp(hdr,"%PDF-",5)==0, "pdf header valid");

    wubulayout_destroy(L);
    wubumodel_doc_destroy(doc);
    if (fails==0) printf("EXP TESTS PASSED\n"); else printf("EXP TESTS FAILED (%d)\n", fails);
    return fails?1:0;
}
