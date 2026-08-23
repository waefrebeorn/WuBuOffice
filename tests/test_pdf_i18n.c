/* test_pdf_i18n.c -- hop 14: non-Latin runs export through the Unicode
 * (Type0/Identity-H + UTF-16BE hex) path, preserving the TEXT LAYER for
 * copy/paste and search. */
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
    (void)u;(void)run; *fs=12;*b=0;*i=0;*d=WUBULAYOUT_LTR; return 1;
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    ck(doc != NULL, "model alloc");
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    wubumodel_node *par = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_node_append(doc, sec, par);
    wubumodel_node *run = wubumodel_node_create(doc, WUBUMODEL_RUN);
    /* "Hello " + 世界 in UTF-8 */
    const char utf8[] = "Hello \xe4\xb8\x96\xe7\x95\x8c";
    wubumodel_run_set_text(run, utf8);
    wubumodel_node_append(doc, par, run);

    wubulayout_doc *L = wubulayout_create(doc, NULL, g_measure, g_style, NULL,
                                          768,1024,72,72,72,72);
    ck(L != NULL, "layout alloc");
    ck(wubuexp_pdf_geometry(L, "/tmp/wb_i18n.pdf") == 0, "export ok");
    wubulayout_destroy(L);

    FILE *f = fopen("/tmp/wb_i18n.pdf", "rb");
    ck(f != NULL, "pdf written");
    fseek(f, 0, SEEK_END); long fsz = ftell(f); fseek(f, 0, SEEK_SET);
    /* N1: FontFile2 is now SUBSET to used codepoints + Flate-compressed.
     * Full font was 760KB; subset for a small doc lands ~40-60KB. */
    ck(fsz > 20000 && fsz < 300000,
       "subset+compressed embedded font keeps PDF small");
    char buf[32768]; size_t n = fread(buf,1,sizeof buf-1,f); buf[n]=0; fclose(f);
    n = 0; (void)n;

    /* the non-Latin run must be a UTF-16BE hex string on font F4:
     * "Hello " is latin (F1 path), 世界 becomes hex 4E16 754C via F4. */
    ck(strstr(buf, "/F4 ") != NULL || strstr(buf, "/F4 ") != NULL,
       "F4 Type0 font declared");
    ck(strstr(buf, "/Identity-H") != NULL, "Identity-H encoding present");
    ck(strstr(buf, "<4E16754C>") != NULL,
       "世界 encoded as UTF-16BE hex (4E16 754C)");
    /* hop 15: real font program embedded for the Type0 font */
    ck(strstr(buf, "/Subtype /CIDFontType2") != NULL, "CIDFontType2 descendant");
    ck(strstr(buf, "/FontFile2") != NULL, "FontFile2 stream (embedded program)");
    ck(strstr(buf, "(Hello ) Tj") == NULL && strstr(buf, "(Hello") != NULL,
       "latin part still emitted");

    fprintf(stderr, bad ? "PDF_I18N FAIL\n" : "PDF_I18N PASS\n");
    return bad ? 1 : 0;
}
