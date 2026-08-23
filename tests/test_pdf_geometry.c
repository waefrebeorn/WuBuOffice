/* test_pdf_geometry.c -- hop 8 fidelity row: geometry-aware PDF export.
 * A model with a Heading1 + body paragraph must export to a PDF whose
 * content stream preserves per-run font sizes and bold (the legacy path
 * reflowed everything to 12pt Helvetica). */
#include "../../src/wubumodel/model.h"
#include "../../src/wubuexp/exp.h"
#include "../../src/wubulayout/ublayout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}


static int g_measure(const char *t, size_t len, int fs,int b,int i,int *h,void*u){
    (void)fs;(void)b;(void)i;(void)u; *h=24; return (int)len*7;
}
static int g_style(void *u, void *run, int *fs,int*b,int*i,wubulayout_dir*d){
    (void)u;(void)run;
    *fs=20; *b=0; *i=0; *d=WUBULAYOUT_LTR;
    return 1;
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    ck(doc != NULL, "model alloc");

    /* heading paragraph with bold run at 20pt via named style */
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    wubumodel_node *par = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_node_append(doc, sec, par);
    wubumodel_node_apply_named_style(par, "Heading1");
    wubumodel_node *run = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "Quarterly Report");
    wubumodel_node_append(doc, par, run);

    wubulayout_doc *L = wubulayout_create(doc, NULL, g_measure, g_style, NULL,
                                          768,1024,72,72,72,72);
    ck(wubuexp_pdf_geometry(L, "/tmp/wb_geom.pdf") == 0,
       "geometry PDF export ok");
    wubulayout_destroy(L);

    FILE *f = fopen("/tmp/wb_geom.pdf", "rb");
    ck(f != NULL, "pdf file written");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long fsz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)fsz + 1);
    size_t n = fread(buf, 1, (size_t)fsz, f); buf[n]=0; fclose(f);
    (void)n;

    ck(strstr(buf, "%PDF-1.4") == buf, "valid PDF header");
    ck(strstr(buf, "/Helvetica-Bold") != NULL, "bold font resource present");
    ck(strstr(buf, "/F2 20 Tf") != NULL || strstr(buf, "Tf 20") != NULL
       || strstr(buf, " 20 Tf") != NULL,
       "heading font size preserved (not flattened to 12)");
    ck(strstr(buf, "(Quarterly) Tj") != NULL && strstr(buf, "(Report) Tj") != NULL,
       "text content present (word-level runs)");
    ck(fsz > 100 && strstr(buf + fsz - 32, "%%EOF") != NULL,
       "PDF terminated");   /* search the tail: binary font bytes contain NULs */
    ck(n > 500, "non-trivial output");

    free(buf);
    wubumodel_doc_destroy(doc);
    fprintf(stderr, bad ? "PDF_GEOMETRY FAIL\n" : "PDF_GEOMETRY PASS\n");
    return bad ? 1 : 0;
}
