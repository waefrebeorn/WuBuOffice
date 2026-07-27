/* test_toc.c -- DOC-54 TOC generator + UXA-41 high-contrast settings. */
#include "toc.h"
#include "model.h"
#include "ublayout.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* unlink (DOC-76 round-trip temp file) */

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
    int w=0; for(size_t k=0;k<len;k++) w += (t[k]==' '?4:7); if (h) *h=12+4; return w;
}
static int sty(void *u, void *run, int *fs,int *b,int *it,wubulayout_dir *d){
    (void)u; *fs=12; *b=0; *it=0; *d=WUBULAYOUT_LTR;
    /* honor the same style props the real Document view callback does */
    wubumodel_style *st = wubumodel_node_style((wubumodel_node*)run);
    if (!st) st = wubumodel_node_style(wubumodel_node_parent((wubumodel_node*)run));
    if (st){
        const char *v;
        if ((v=wubumodel_style_get_prop(st,"size"))) *fs = atoi(v);
        if ((v=wubumodel_style_get_prop(st,"bold")) && (v[0]=='1'||v[0]=='t')) *b=1;
        if ((v=wubumodel_style_get_prop(st,"italic")) && (v[0]=='1'||v[0]=='t')) *it=1;
    }
    return 1;
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

    /* DOC-55: footnotes / endnotes */
    {
        wubumodel_doc *d = wubumodel_doc_create();
        wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
        /* footnote / endnote are block-level (siblings of paragraphs) in this
         * layout: lay_node visits them via the generic recursion. */
        wubumodel_node *para = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
        wubumodel_node *r0 = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(r0, "Body text.");
        wubumodel_node_append(d, para, r0);
        wubumodel_node_append(d, sec, para);

        wubumodel_node *mark = wubumodel_node_create(d, WUBUMODEL_FOOTNOTE);
        wubumodel_node *r = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(r, "1");
        wubumodel_node_append(d, mark, r);
        wubumodel_node_set_note(mark, "First footnote body text.");
        wubumodel_node_append(d, sec, mark);

        wubumodel_node *para2 = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
        wubumodel_node *r1 = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(r1, "More body.");
        wubumodel_node_append(d, para2, r1);
        wubumodel_node_append(d, sec, para2);

        wubumodel_node *en = wubumodel_node_create(d, WUBUMODEL_ENDNOTE);
        wubumodel_node *r2 = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(r2, "A");
        wubumodel_node_append(d, en, r2);
        wubumodel_node_set_note(en, "Endnote body.");
        wubumodel_node_append(d, sec, en);

        /* DOC-60: hyperlink */
        wubumodel_node *lk = wubumodel_node_create(d, WUBUMODEL_LINK);
        wubumodel_node *lr = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(lr, "Visit site");
        wubumodel_node_append(d, lk, lr);
        wubumodel_node_set_link(lk, "https://example.com");
        wubumodel_node_append(d, sec, lk);

        /* note marker text survives */
        CK(strcmp(wubumodel_run_text(r), "1")==0, "footnote marker text");
        CK(strcmp(wubumodel_node_note(mark), "First footnote body text.")==0, "footnote body stored");
        /* collected notes in document order */
        const char **arr = NULL;
        int n = wubumodel_doc_notes(d, &arr);
        CK(n == 2, "two notes collected");
        if (n == 2){
            CK(strcmp(arr[0], "First footnote body text.")==0, "note[0] order");
            CK(strcmp(arr[1], "Endnote body.")==0, "note[1] order");
        }
        free(arr);

        /* layout emits a marker run for the footnote */
        wubulayout_doc *L = wubulayout_create(d, sec, meas, sty, NULL, 400, 400, 20,20,20,20);
        CK(L != NULL, "footnote doc lays out");
        int mark_runs = 0;
        for (int pg=0; pg<wubulayout_page_count(L); pg++)
            for (int i=0;i<wubulayout_run_count(L,pg); i++){
                const wubulayout_run *ru = wubulayout_run_at(L, pg, i);
                if (ru && ru->user == (void*)mark) mark_runs++;
            }
        CK(mark_runs == 1, "footnote marker laid out once");

        /* DOC-60: link target stored + run's parent is the link + laid out */
        CK(strcmp(wubumodel_node_link(lk), "https://example.com")==0, "link target stored");
        CK(wubumodel_node_parent(lr) == lk, "link run parent is the link node");
        wubulayout_destroy(L);
        wubulayout_doc *L2 = wubulayout_create(d, sec, meas, sty, NULL, 400, 400, 20,20,20,20);
        int link_runs = 0;
        for (int pg=0; pg<wubulayout_page_count(L2); pg++)
            for (int i=0;i<wubulayout_run_count(L2,pg); i++){
                const wubulayout_run *ru = wubulayout_run_at(L2, pg, i);
                if (!ru || !ru->user) continue;
                wubumodel_node *rn = (wubumodel_node*)ru->user;
                wubumodel_node *par = wubumodel_node_parent(rn);
                if (par == lk || rn == lk) link_runs++;
            }
        CK(link_runs == 1, "link laid out once");
        wubulayout_destroy(L2);

        /* DOC-59: bullet list paragraph laying out a bullet prefix run. */
        wubumodel_node *li = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
        wubumodel_style *ls = wubumodel_style_create();
        wubumodel_style_set_prop(ls, "list", "bullet");
        wubumodel_node_set_style(li, ls);
        wubumodel_style_destroy(ls);
        wubumodel_node *lir = wubumodel_node_create(d, WUBUMODEL_RUN);
        wubumodel_run_set_text(lir, "An item");
        wubumodel_node_append(d, li, lir);
        wubumodel_node_append(d, sec, li);
        wubulayout_doc *L3 = wubulayout_create(d, sec, meas, sty, NULL, 400, 400, 20,20,20,20);
        int bullet_runs = 0, item_runs = 0;
        for (int pg=0; pg<wubulayout_page_count(L3); pg++)
            for (int i=0;i<wubulayout_run_count(L3,pg); i++){
                const wubulayout_run *ru = wubulayout_run_at(L3, pg, i);
                if (!ru || !ru->text || !ru->text_len) continue;
                if (ru->text[0]==(char)0xe2 && ru->text[1]==(char)0x80 && ru->text[2]==(char)0xa2) bullet_runs++;
                if (ru->user == (void*)lir) item_runs++;
            }
        CK(bullet_runs >= 1, "bullet prefix emitted");
        CK(item_runs >= 1, "list item text laid out");
        wubulayout_destroy(L3);

        /* DOC-62: table of cells lays out cell boxes + cell text. */
        wubumodel_node *tbl = wubumodel_node_create(d, WUBUMODEL_TABLE);
        for (int r=0; r<2; r++){
            wubumodel_node *cell = wubumodel_node_create(d, WUBUMODEL_CELL);
            wubumodel_node *cp = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
            wubumodel_node *cr = wubumodel_node_create(d, WUBUMODEL_RUN);
            char cbuf[16]; snprintf(cbuf,sizeof cbuf,"Cell%d", r+1);
            wubumodel_run_set_text(cr, cbuf);
            wubumodel_node_append(d, cp, cr);
            wubumodel_node_append(d, cell, cp);
            wubumodel_node_append(d, tbl, cell);
        }
        wubumodel_node_append(d, sec, tbl);
        wubulayout_doc *L4 = wubulayout_create(d, sec, meas, sty, NULL, 400, 400, 20,20,20,20);
        int cell_boxes = 0;
        for (int pg=0; pg<wubulayout_page_count(L4); pg++)
            for (int i=0;i<wubulayout_box_count(L4,pg); i++){
                const wubulayout_box *bx = wubulayout_box_at(L4, pg, i);
                if (bx && bx->user) cell_boxes++;
            }
        CK(cell_boxes == 2, "two cell boxes laid out");
        wubulayout_destroy(L4);

        /* DOC-61: image node retains its RGBA and lays out as an object box. */
        wubumodel_node *im = wubumodel_node_create(d, WUBUMODEL_IMAGE);
        const int IW=8, IH=4; uint8_t pix[IW*IH*4];
        for (int i=0;i<IW*IH;i++){ pix[i*4]=10; pix[i*4+1]=20; pix[i*4+2]=30; pix[i*4+3]=255; }
        CK(wubumodel_node_set_image(im, pix, IW, IH)==0, "image set");
        int gw=0, gh=0; const uint8_t *gp = wubumodel_node_image(im, &gw, &gh);
        CK(gp && gw==IW && gh==IH, "image retrieved");
        wubumodel_node_append(d, sec, im);
        wubulayout_doc *L5 = wubulayout_create(d, sec, meas, sty, NULL, 400, 400, 20,20,20,20);
        int img_boxes = 0;
        for (int pg=0; pg<wubulayout_page_count(L5); pg++)
            for (int i=0;i<wubulayout_box_count(L5,pg); i++){
                const wubulayout_box *bx = wubulayout_box_at(L5, pg, i);
                if (bx && bx->user==(void*)im) img_boxes++;
            }
        CK(img_boxes == 1, "image laid out as a box");
        wubulayout_destroy(L5);

        /* DOC-57: page break creates a second page. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *p1 = wubumodel_node_create(dd, WUBUMODEL_PARAGRAPH);
            wubumodel_node *r1 = wubumodel_node_create(dd, WUBUMODEL_RUN);
            wubumodel_run_set_text(r1,"A"); wubumodel_node_append(dd,p1,r1);
            wubumodel_node_append(dd,ss,p1);
            wubumodel_node *pb = wubumodel_node_create(dd, WUBUMODEL_PAGEBREAK);
            wubumodel_node_set_break(pb,0); wubumodel_node_append(dd,ss,pb);
            wubumodel_node *p2 = wubumodel_node_create(dd, WUBUMODEL_PARAGRAPH);
            wubumodel_node *r2 = wubumodel_node_create(dd, WUBUMODEL_RUN);
            wubumodel_run_set_text(r2,"B"); wubumodel_node_append(dd,p2,r2);
            wubumodel_node_append(dd,ss,p2);
            wubulayout_doc *Lb = wubulayout_create(dd, ss, meas, sty, NULL, 400, 400, 20,20,20,20);
            CK(wubulayout_page_count(Lb) >= 2, "page break forces 2nd page");
            wubulayout_destroy(Lb); wubumodel_doc_destroy(dd);
        }
        /* DOC-56: header/footer nodes are recognized and skipped in body flow. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *hd = wubumodel_node_create(dd, WUBUMODEL_HEADER);
            wubumodel_node_set_text(hd,"My Header"); wubumodel_node_append(dd,ss,hd);
            wubumodel_node *ft = wubumodel_node_create(dd, WUBUMODEL_FOOTER);
            wubumodel_node_set_text(ft,"My Footer"); wubumodel_node_append(dd,ss,ft);
            CK(wubumodel_node_kind(hd)==WUBUMODEL_HEADER, "header kind");
            CK(strcmp(wubumodel_node_text(hd),"My Header")==0, "header text");
            CK(wubumodel_node_kind(ft)==WUBUMODEL_FOOTER, "footer kind");
            wubumodel_doc_destroy(dd);
        }
        /* DOC-63: comment node carries author + text. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *c = wubumodel_node_create(dd, WUBUMODEL_COMMENT);
            wubumodel_node_set_text(c,"Fix this"); wubumodel_node_set_author(c,"Al");
            wubumodel_node_append(dd,ss,c);
            CK(strcmp(wubumodel_node_author(c),"Al")==0, "comment author");
            wubulayout_doc *Lc = wubulayout_create(dd, ss, meas, sty, NULL, 400, 400, 20,20,20,20);
            int cr=0; for (int pg=0;pg<wubulayout_page_count(Lc);pg++)
                for (int i=0;i<wubulayout_run_count(Lc,pg);i++)
                    if (wubulayout_run_at(Lc,pg,i)->user==(void*)c) cr++;
            CK(cr>=1, "comment laid out");
            wubulayout_destroy(Lc); wubumodel_doc_destroy(dd);
        }
        /* DOC-64: track-change node carries type + text. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *t = wubumodel_node_create(dd, WUBUMODEL_TRACKCHANGE);
            wubumodel_node_set_text(t,"new"); wubumodel_node_set_tc(t,0);
            wubumodel_node_append(dd,ss,t);
            CK(wubumodel_node_tc(t)==0, "trackchange type stored");
            wubulayout_doc *Lt = wubulayout_create(dd, ss, meas, sty, NULL, 400, 400, 20,20,20,20);
            int tr=0; for (int pg=0;pg<wubulayout_page_count(Lt);pg++)
                for (int i=0;i<wubulayout_run_count(Lt,pg);i++)
                    if (wubulayout_run_at(Lt,pg,i)->user==(void*)t) tr++;
            CK(tr>=1, "trackchange laid out");
            wubulayout_destroy(Lt); wubumodel_doc_destroy(dd);
        }
        /* DOC-65: field node carries kind + value. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *f = wubumodel_node_create(dd, WUBUMODEL_FIELD);
            wubumodel_node_set_field(f,"date"); wubumodel_node_set_text(f,"2026-07-27");
            wubumodel_node_append(dd,ss,f);
            CK(strcmp(wubumodel_node_field(f),"date")==0, "field kind stored");
            wubulayout_doc *Lf = wubulayout_create(dd, ss, meas, sty, NULL, 400, 400, 20,20,20,20);
            int fr=0; for (int pg=0;pg<wubulayout_page_count(Lf);pg++)
                for (int i=0;i<wubulayout_run_count(Lf,pg);i++)
                    if (wubulayout_run_at(Lf,pg,i)->user==(void*)f) fr++;
            CK(fr>=1, "field laid out");
            wubulayout_destroy(Lf); wubumodel_doc_destroy(dd);
        }
        /* DOC-58: named-style presets apply heading/size props end-to-end. */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *ss = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *p = wubumodel_node_create(dd, WUBUMODEL_PARAGRAPH);
            wubumodel_node *r = wubumodel_node_create(dd, WUBUMODEL_RUN);
            wubumodel_run_set_text(r,"Title"); wubumodel_node_append(dd,p,r);
            wubumodel_node_append(dd,ss,p);
            CK(wubumodel_node_apply_named_style(p,"Heading1")==0, "apply Heading1");
            wubumodel_style *st = wubumodel_node_style(p);
            CK(st && strcmp(wubumodel_style_get_prop(st,"heading"),"1")==0, "heading=1 prop");
            CK(st && strcmp(wubumodel_style_get_prop(st,"size"),"26")==0, "size=26 prop");
            /* layout must report the larger font size via the style callback */
            wubulayout_doc *Lst = wubulayout_create(dd, ss, meas, sty, NULL, 400, 400, 20,20,20,20);
            int big=0; for (int pg=0;pg<wubulayout_page_count(Lst);pg++)
                for (int i=0;i<wubulayout_run_count(Lst,pg);i++){
                    const wubulayout_run *R = wubulayout_run_at(Lst,pg,i);
                    if (R && R->user==(void*)r && R->font_size>=26) big++;
                }
            CK(big>=1, "heading font size reflected in layout");
            wubulayout_destroy(Lst); wubumodel_doc_destroy(dd);
        }
        /* DOC-76: DOCX round-trip fidelity through the real model-io path the
         * UI uses (write_docx -> load_docx -> inspect reloaded model). */
        {
            wubumodel_doc *dd = wubumodel_doc_create();
            wubumodel_node *sec = wubumodel_node_create(dd, WUBUMODEL_SECTION);
            wubumodel_node *h = wubumodel_node_create(dd, WUBUMODEL_PARAGRAPH);
            wubumodel_node *hr = wubumodel_node_create(dd, WUBUMODEL_RUN);
            wubumodel_run_set_text(hr,"Round Trip Title"); wubumodel_node_append(dd,h,hr);
            wubumodel_node_apply_named_style(h,"Heading1");
            wubumodel_node *p = wubumodel_node_create(dd, WUBUMODEL_PARAGRAPH);
            wubumodel_node *pr = wubumodel_node_create(dd, WUBUMODEL_RUN);
            wubumodel_run_set_text(pr,"The quick brown fox jumps."); wubumodel_node_append(dd,p,pr);
            wubumodel_node_append(dd,sec,h); wubumodel_node_append(dd,sec,p);
            const char *tmp = "/tmp/wubuos_rt76.docx";
            CK(wubumodel_write_docx(dd, tmp)==0, "docx write");
            wubumodel_doc_destroy(dd);
            wubumodel_doc *back = NULL;
            CK(wubumodel_load_docx(tmp, &back)==0 && back, "docx read back");
            /* walk reloaded model, collect run text, assert fidelity.
             * wubumodel_doc_root() returns the top-level SECTION (first
             * parentless node), whose children are the paragraphs. */
            int paras=0; char buf[1024]; buf[0]=0;
            wubumodel_node *rsec = wubumodel_doc_root(back);
            for (wubumodel_node *n = rsec?wubumodel_node_first_child(rsec):NULL; n;
                 n = wubumodel_node_next_sibling(n)){
                if (wubumodel_node_kind(n)!=WUBUMODEL_PARAGRAPH) continue;
                paras++;
                for (wubumodel_node *r = wubumodel_node_first_child(n); r;
                     r = wubumodel_node_next_sibling(r)){
                    if (wubumodel_node_kind(r)==WUBUMODEL_RUN){
                        const char *t = wubumodel_run_text(r);
                        if (t){ strncat(buf, t, sizeof buf-1-strlen(buf)); }
                    }
                }
            }
            CK(paras>=2, "reloaded paragraphs preserved");
            CK(strstr(buf,"Round Trip Title")!=NULL, "heading text survived round-trip");
            CK(strstr(buf,"quick brown fox")!=NULL, "fox sentence survived round-trip");
            wubumodel_doc_destroy(back);
            unlink(tmp);
        }
        wubumodel_doc_destroy(d);
    }
    if (fails==0) printf("TOC + SETTINGS + FOOTNOTE TESTS PASSED\n");
    else printf("TOC + SETTINGS + FOOTNOTE TESTS FAILED (%d)\n", fails);
    return fails?1:0;
}
