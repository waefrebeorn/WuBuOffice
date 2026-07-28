/* doccmd.c -- opaque document-editing command module (see doccmd.h).
 * Ported out of view_doc.c so the Document view no longer owns 15 insert
 * commands. Each command is self-contained and returns what the view must own.
 */
#include "doccmd.h"

#include "a11y.h"      /* a11y_check_doc, a11y_report */
#include "chart.h"     /* wubuchart */
#include "draw.h"      /* wubudraw */
#include "math.h"      /* wubumath */
#include "epub.h"      /* wubuepub */
#include "script.h"    /* wubuscript: script_eval */
#include "wubusvg/rast.h"  /* svg_rasterize_cb */
#include "qr.h"        /* wubuocr: qr_encode */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- object inserts: build sample, rasterize, return malloc'd RGBA ---- */
static unsigned char *raster_obj(const char *svg, svg_text_fn tf, int *w, int *h){
    if (!svg) return NULL;
    unsigned char *fb = NULL; int rw=0, rh=0;
    int rc = svg_rasterize_cb(svg, strlen(svg), &fb, &rw, &rh, tf);
    free((void*)svg);
    if (!rc) return NULL;
    *w = rw; *h = rh;
    return fb;
}

unsigned char *doccmd_insert_chart(svg_text_fn tf, int *w, int *h){
    Chart *c = chart_create("Sample Bar Chart");
    chart_set_type(c, CHART_BAR);
    chart_set_size(c, 320, 200);
    double ys[4] = { 23, 41, 17, 52 };
    const char *lbl[4] = { "Q1", "Q2", "Q3", "Q4" };
    chart_add_series(c, "sales", ys, 4, lbl);
    char *svg = chart_render_svg(c);
    chart_free(c);
    return raster_obj(svg, tf, w, h);
}

unsigned char *doccmd_insert_draw(svg_text_fn tf, int *w, int *h){
    DrawScene *s = draw_create(320, 200);
    draw_add_rect(s, 20, 20, 120, 80, "#4488cc", "#224466");
    draw_add_ellipse(s, 240, 100, 50, 40, "#cc6644", "none");
    draw_add_line(s, 20, 180, 300, 180, "#333333", 2);
    draw_add_text(s, 30, 60, "Draw", 20, "#ffffff");
    char *svg = draw_render_svg(s);
    draw_destroy(s);
    return raster_obj(svg, tf, w, h);
}

unsigned char *doccmd_insert_math(svg_text_fn tf, int *w, int *h){
    char *svg = math_render_svg("x^2 + 1 = \\frac{a}{b}");
    return raster_obj(svg, tf, w, h);
}

/* ---- structural inserts (return 1 if TOC needs rebuild) ---- */
/* The document's top-level section is the first parentless node (doc_root).
 * (A latent bug had this return first_child(doc_root) -- the first paragraph
 * -- so inserts nested under the first paragraph instead of the section.) */
static wubumodel_node *first_section(wubumodel_doc *doc){
    return doc ? wubumodel_doc_root(doc) : NULL;
}

int doccmd_insert_link(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *lk = wubumodel_node_create(doc, WUBUMODEL_LINK);
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, "WuBuOffice");
    wubumodel_node_append(doc, lk, r);
    wubumodel_node_set_link(lk, "https://github.com/waefrebeorn/WuBuOffice");
    wubumodel_node_append(doc, sec, lk);
    return 1;
}

int doccmd_insert_list(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *p = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_style *st = wubumodel_style_create();
    wubumodel_style_set_prop(st, "list", "bullet");
    wubumodel_node_set_style(p, st);
    wubumodel_style_destroy(st);
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, "List item");
    wubumodel_node_append(doc, p, r);
    wubumodel_node_append(doc, sec, p);
    return 1;
}

int doccmd_insert_table(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *tbl = wubumodel_node_create(doc, WUBUMODEL_TABLE);
    for (int r=0; r<2; r++){
        wubumodel_node *cell = wubumodel_node_create(doc, WUBUMODEL_CELL);
        wubumodel_node *para = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
        wubumodel_node *rr = wubumodel_node_create(doc, WUBUMODEL_RUN);
        char buf[32]; snprintf(buf,sizeof buf,"Cell %d", r+1);
        wubumodel_run_set_text(rr, buf);
        wubumodel_node_append(doc, para, rr);
        wubumodel_node_append(doc, cell, para);
        wubumodel_node_append(doc, tbl, cell);
    }
    wubumodel_node_append(doc, sec, tbl);
    return 1;
}

int doccmd_insert_image(wubumodel_doc *doc){
    return doccmd_insert_image_alt(doc, NULL) ? 1 : 0;
}

/* ---- arg-bearing variants (driven by the shell's modal dialogs) ---- */
int doccmd_insert_link_url(wubumodel_doc *doc, const char *url){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    const char *u = (url && *url) ? url : "https://github.com/waefrebeorn/WuBuOffice";
    wubumodel_node *lk = wubumodel_node_create(doc, WUBUMODEL_LINK);
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    /* label = the URL itself (readable run) */
    const char *label = (url && *url) ? url : "WuBuOffice";
    wubumodel_run_set_text(r, label);
    wubumodel_node_append(doc, lk, r);
    wubumodel_node_set_link(lk, u);
    wubumodel_node_append(doc, sec, lk);
    return 1;
}

int doccmd_insert_image_alt(wubumodel_doc *doc, const char *alt){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *im = wubumodel_node_create(doc, WUBUMODEL_IMAGE);
    const int W=96, H=64;
    uint8_t *px = malloc((size_t)W*H*4);
    if (!px) return 0;
    for (int y=0; y<H; y++)
        for (int x=0; x<W; x++){
            uint8_t *p = px + ((size_t)y*W+x)*4;
            int chk = ((x/8)+(y/8)) & 1;
            p[0] = chk? 60:220; p[1] = chk? 140:80; p[2] = chk? 220:60; p[3] = 255;
        }
    wubumodel_node_set_image(im, px, W, H);
    free(px);
    /* alt text -> accessibility note (model has no dedicated alt field) */
    if (alt && *alt) wubumodel_node_set_note(im, alt);
    wubumodel_node_append(doc, sec, im);
    return 1;
}

int doccmd_insert_qr(wubumodel_doc *doc, const char *text){
    wubumodel_node *sec = first_section(doc);
    if (!sec || !text || !*text) return 0;
    unsigned char *matrix = NULL; int sz = 0;
    if (qr_encode(text, &matrix, &sz) < 0 || !matrix || sz <= 0){ free(matrix); return 0; }
    /* render the module matrix to a scaled RGBA bitmap (module = 4px) */
    const int S = 4;
    int W = sz*S, H = sz*S;
    uint8_t *px = malloc((size_t)W*H*4);
    if (!px){ free(matrix); return 0; }
    for (int y=0; y<H; y++)
        for (int x=0; x<W; x++){
            int mx = x/S, my = y/S;
            int on = (mx < sz && my < sz) ? matrix[my*sz + mx] : 0;
            uint8_t *p = px + ((size_t)y*W+x)*4;
            p[0]=p[1]=p[2] = on? 0 : 255; p[3] = 255;
        }
    wubumodel_node *im = wubumodel_node_create(doc, WUBUMODEL_IMAGE);
    wubumodel_node_set_image(im, px, W, H);
    free(px);
    wubumodel_node_set_note(im, text);   /* QR payload as alt text */
    wubumodel_node_append(doc, sec, im);
    free(matrix);
    return 1;
}

int doccmd_insert_pagebreak(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *b = wubumodel_node_create(doc, WUBUMODEL_PAGEBREAK);
    wubumodel_node_set_break(b, 0);
    wubumodel_node_append(doc, sec, b);
    return 1;
}

int doccmd_insert_sectionbreak(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *b = wubumodel_node_create(doc, WUBUMODEL_SECTIONBREAK);
    wubumodel_node_set_break(b, 1);
    wubumodel_node_append(doc, sec, b);
    return 1;
}

int doccmd_insert_header(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    for (wubumodel_node *n = wubumodel_node_first_child(sec); n; n = wubumodel_node_next_sibling(n))
        if (wubumodel_node_kind(n)==WUBUMODEL_HEADER){ wubumodel_node_set_text(n, "WuBuOffice Header"); return 1; }
    wubumodel_node *hd = wubumodel_node_create(doc, WUBUMODEL_HEADER);
    wubumodel_node_set_text(hd, "WuBuOffice Header");
    wubumodel_node_append(doc, sec, hd);
    return 1;
}

int doccmd_insert_footer(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    for (wubumodel_node *n = wubumodel_node_first_child(sec); n; n = wubumodel_node_next_sibling(n))
        if (wubumodel_node_kind(n)==WUBUMODEL_FOOTER){ wubumodel_node_set_text(n, "Page footer"); return 1; }
    wubumodel_node *ft = wubumodel_node_create(doc, WUBUMODEL_FOOTER);
    wubumodel_node_set_text(ft, "Page footer");
    wubumodel_node_append(doc, sec, ft);
    return 1;
}

int doccmd_insert_comment(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *c = wubumodel_node_create(doc, WUBUMODEL_COMMENT);
    wubumodel_node_set_text(c, "Please review this clause.");
    wubumodel_node_set_author(c, "Reviewer");
    wubumodel_node_append(doc, sec, c);
    return 1;
}

int doccmd_insert_trackchange(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *t = wubumodel_node_create(doc, WUBUMODEL_TRACKCHANGE);
    wubumodel_node_set_text(t, "proposed insertion");
    wubumodel_node_set_tc(t, 0);
    wubumodel_node_set_author(t, "Editor");
    wubumodel_node_append(doc, sec, t);
    return 1;
}

int doccmd_insert_field(wubumodel_doc *doc){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    wubumodel_node *f = wubumodel_node_create(doc, WUBUMODEL_FIELD);
    wubumodel_node_set_field(f, "date");
    wubumodel_node_set_text(f, "2026-07-27");
    wubumodel_node_append(doc, sec, f);
    return 1;
}

/* DOC-97: expose the wubuscript formula host as a computed field. */
static int doccmd_script_resolve(const char *name, double *out, void *ctx){
    wubumodel_doc *doc = ctx;
    if (!strcmp(name, "lines")){
        int n = 0;
        /* count every paragraph anywhere in the doc (recursive) */
        for (wubumodel_node *sec = wubumodel_doc_root(doc);
             sec; sec = wubumodel_node_next_sibling(sec)){
            wubumodel_node *stk[256]; int sp=0; stk[sp++]=sec;
            while (sp){
                wubumodel_node *cur = stk[--sp];
                if (wubumodel_node_kind(cur)==WUBUMODEL_PARAGRAPH) n++;
                for (wubumodel_node *c=wubumodel_node_first_child(cur); c; c=wubumodel_node_next_sibling(c))
                    if (sp<256) stk[sp++]=c;
            }
        }
        *out = (double)n; return 0;
    }
    (void)name; return -1;
}

int doccmd_insert_script_field(wubumodel_doc *doc, const char *expr){
    wubumodel_node *sec = first_section(doc);
    if (!sec) return 0;
    double v = 0;
    char val[64];
    if (script_eval(expr, doccmd_script_resolve, doc, &v) != 0) v = 0;
    snprintf(val, sizeof val, "%.4g", v);
    wubumodel_node *f = wubumodel_node_create(doc, WUBUMODEL_FIELD);
    wubumodel_node_set_field(f, "script");
    wubumodel_node_set_text(f, val);
    wubumodel_node_append(doc, sec, f);
    return 1;
}

char *doccmd_export_epub(wubumodel_doc *doc, const char *out){
    if (!doc) return strdup("no model doc to export");
    int rc = epub_write(doc, out, "WuBuOffice Document", "en");
    if (rc==0){
        char b[128]; snprintf(b,sizeof b,"EPUB written: %s", out);
        return strdup(b);
    }
    return strdup("EPUB export failed");
}

char *doccmd_save(wubumodel_doc *doc, const char *path){
    if (!doc) return strdup("no model doc to save");
    char out[512];
    if (path && (strstr(path,".docx")||strstr(path,".docm")||strstr(path,".dotx")))
        snprintf(out,sizeof out,"%s", path);
    else if (path && (strstr(path,".odt")||strstr(path,".fodt")))
        snprintf(out,sizeof out,"%s", path);
    else if (path)
        snprintf(out,sizeof out,"%s.docx", path);
    else
        snprintf(out,sizeof out,"/tmp/wubuos_document.docx");
    int rc;
    if (strstr(out,".odt")||strstr(out,".fodt")) rc = wubumodel_write_odt(doc, out);
    else rc = wubumodel_write_docx(doc, out);
    if (rc==0){
        char b[512]; const char *lab = (strstr(out,".odt")||strstr(out,".fodt"))?"ODT":"DOCX";
        snprintf(b,sizeof b,"%s saved: %s", lab, out);
        return strdup(b);
    }
    return strdup("DOCX/ODT save failed");
}

void doccmd_a11y_check(wubumodel_doc *doc, a11y_report *out){
    if (out){ out->count=0; out->items=NULL; out->cap=0; }
    if (doc) a11y_check_doc(doc, 1, 0, out);
}
