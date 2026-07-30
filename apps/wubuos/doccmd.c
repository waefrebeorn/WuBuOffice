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
#include "../apps/wubupdf/pdf.h"  /* wubupdf_write (dm_doc-based PDF/1.7 writer) */
#include "exp.h"       /* wubuexp_pdf/html/markdown/latex/rtf (INT-3.5) */
#include "rtf.h"       /* rtf_write (run-based RTF writer from src/wuburtf) */
#include "redact.h"    /* redact_create/mark/apply (redaction engine) */
#include "col.h"       /* col_create/add/reply/resolve (comment-thread store) */
#include "script.h"    /* wubuscript: script_eval */
#include "ublayout.h"  /* wubulayout_create (central pipeline -> export) */
#include "wuos_font.h" /* wuos_font_text_width / _height for measure callback */
#include "model.h"     /* wubumodel_doc_root */
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
        char b[768]; const char *lab = (strstr(out,".odt")||strstr(out,".fodt"))?"ODT":"DOCX";
        snprintf(b,sizeof b,"%s saved: %s", lab, out);
        return strdup(b);
    }
    return strdup("DOCX/ODT save failed");
}

void doccmd_a11y_check(wubumodel_doc *doc, a11y_report *out){
    if (out){ out->count=0; out->items=NULL; out->cap=0; }
    if (doc) a11y_check_doc(doc, 1, 0, out);
}

/* ---- layout-based exporters (INT-3.5) ----
 * wubuexp takes a `wubulayout_doc *L` (a laid-out document) and writes a
 * PDF/HTML/Markdown/LaTeX/RTF file. The doccmd exporters below build a
 * layout from the model doc via measure/style callbacks, then call wubuexp.
 * Pages: A4-ish @ ~768x1024, default 72-pt margins.
 *
 * The measure callback deliberately does NOT depend on the app font
 * (wuos_font_text_width lives in apps/wubuos/wuos_font.c which pulls
 * FreeType and would inflate test_doccmd's link surface). For exports
 * we want to measure STRUCTURE, not visual width; a fixed 7-px-per-byte
 * estimate gives the layout enough info to paginate correctly. Visual
 * fidelity comes from the renderer path, which already uses wuos_font. */
static int doccmd_measure_cb(const char *t, size_t len, int fs, int bold,
                             int italic, int *out_h, void *user){
    (void)bold; (void)italic; (void)user;
    if (out_h) *out_h = fs + 4;
    /* 7 px per byte for ASCII is close to wuos_font's 11pt default; UTF-8
     * multi-byte chars get a small bonus. */
    int nbytes = 0;
    for (size_t i = 0; i < len; i++){
        unsigned char c = (unsigned char)t[i];
        nbytes++;
        if ((c & 0xC0) == 0x80) nbytes--;  /* continuation: count once */
    }
    return (nbytes * 7 * fs) / 14;
}
static int doccmd_style_cb(void *user, void *run, int *fs, int *bold,
                           int *italic, wubulayout_dir *dir){
    /* Conservative defaults: same-size body text, no decorations. The
     * exported PDF/MD/HTML/LATEX/RTF reflects the model's STRUCTURE not
     * its visual styling -- callers who want typographic fidelity should
     * walk the model directly. */
    (void)user; (void)run;
    *fs = 14; *bold = 0; *italic = 0; *dir = WUBULAYOUT_LTR;
    return 0;
}
static wubulayout_doc *doccmd_layout(wubumodel_doc *doc){
    if (!doc) return NULL;
    void *root = wubumodel_doc_root(doc);
    if (!root) root = NULL; /* layout accepts NULL and uses doc root */
    return wubulayout_create(doc, root, doccmd_measure_cb,
                             doccmd_style_cb, NULL,
                             768, 1024, 72, 72, 72, 72);
}
char *doccmd_export_pdf(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    wubulayout_doc *L = doccmd_layout(doc);
    if (!L) return strdup("layout build failed (model empty?)");
    int rc = wubuexp_pdf(L, "/tmp/wubuos_export.pdf");
    wubulayout_destroy(L);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "PDF written: /tmp/wubuos_export.pdf"); return strdup(b); }
    return strdup("PDF export failed");
}
/* Direct dm_doc-based PDF writer (apps/wubupdf/pdf_write.c). Produces a
 * smaller PDF that reflects the model's STRUCTURE (paragraphs + text +
 * tables) without going through the layout pipeline. Useful as a fallback
 * when the layout build fails on malformed models. */
char *doccmd_export_pdf_direct(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    /* Build a minimal dm_doc from the model by walking sections/paragraphs.
     * Avoids the wubuconv bridge dependency to keep the doccmd binary slim. */
    dm_doc dm; memset(&dm, 0, sizeof dm);
    dm.cap = 8;
    dm.blocks = calloc(dm.cap, sizeof *dm.blocks);
    for (wubumodel_node *sec = wubumodel_doc_root(doc); sec; sec = wubumodel_node_next_sibling(sec)){
        /* collect run text under this section */
        char buf[1024]; size_t blen = 0; buf[0] = 0;
        for (wubumodel_node *p = wubumodel_node_first_child(sec); p; p = wubumodel_node_next_sibling(p)){
            for (wubumodel_node *r = wubumodel_node_first_child(p); r; r = wubumodel_node_next_sibling(r)){
                const char *t = wubumodel_run_text(r);
                if (t && *t){
                    size_t l = strlen(t);
                    if (blen + l + 1 < sizeof buf){
                        memcpy(buf + blen, t, l); blen += l; buf[blen] = 0;
                    }
                }
            }
            /* paragraph break */
            if (blen + 1 < sizeof buf){ buf[blen++] = '\n'; buf[blen] = 0; }
        }
        if (blen){
            if (dm.n + 1 > dm.cap){ dm.cap *= 2; dm.blocks = realloc(dm.blocks, dm.cap * sizeof *dm.blocks); }
            dm_block *b = &dm.blocks[dm.n++]; memset(b, 0, sizeof *b);
            b->kind = DM_BLOCK_PARA;
            b->para.text = strdup(buf);
        }
    }
    int rc = wubupdf_write(&dm, "/tmp/wubuos_export_direct.pdf");
    /* free the dm_doc */
    for (size_t i = 0; i < dm.n; i++){
        free(dm.blocks[i].para.text);
        free(dm.blocks[i].para.style);
        if (dm.blocks[i].kind == DM_BLOCK_TABLE && dm.blocks[i].table.cells){
            for (size_t k = 0; k < dm.blocks[i].table.rows * dm.blocks[i].table.cols; k++)
                if (dm.blocks[i].table.cells[k]){
                    free(dm.blocks[i].table.cells[k]->text);
                    free(dm.blocks[i].table.cells[k]->style);
                    free(dm.blocks[i].table.cells[k]);
                }
            free(dm.blocks[i].table.cells);
        }
    }
    free(dm.blocks);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "PDF written (direct): /tmp/wubuos_export_direct.pdf"); return strdup(b); }
    return strdup("PDF direct export failed");
}
char *doccmd_export_html(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    wubulayout_doc *L = doccmd_layout(doc);
    if (!L) return strdup("layout build failed (model empty?)");
    int rc = wubuexp_html(L, "/tmp/wubuos_export.html");
    wubulayout_destroy(L);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "HTML written: /tmp/wubuos_export.html"); return strdup(b); }
    return strdup("HTML export failed");
}
char *doccmd_export_markdown(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    wubulayout_doc *L = doccmd_layout(doc);
    if (!L) return strdup("layout build failed (model empty?)");
    int rc = wubuexp_markdown(L, "/tmp/wubuos_export.md");
    wubulayout_destroy(L);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "Markdown written: /tmp/wubuos_export.md"); return strdup(b); }
    return strdup("Markdown export failed");
}
char *doccmd_export_latex(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    wubulayout_doc *L = doccmd_layout(doc);
    if (!L) return strdup("layout build failed (model empty?)");
    int rc = wubuexp_latex(L, "/tmp/wubuos_export.tex");
    wubulayout_destroy(L);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "LaTeX written: /tmp/wubuos_export.tex"); return strdup(b); }
    return strdup("LaTeX export failed");
}
char *doccmd_export_rtf(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc to export");
    wubulayout_doc *L = doccmd_layout(doc);
    if (!L) return strdup("layout build failed (model empty?)");
    int rc = wubuexp_rtf(L, "/tmp/wubuos_export.rtf");
    wubulayout_destroy(L);
    if (rc == 0){ char b[128]; snprintf(b, sizeof b, "RTF written: /tmp/wubuos_export.rtf"); return strdup(b); }
    return strdup("RTF export failed");
}

/* doccmd_export_rtf_runs -- direct RTF write via src/wuburtf (run-based, no
 * layout pass). Used when caller already has styled runs; skips wubulayout.
 * Writes /tmp/wubuos_export_runs.rtf. */
char *doccmd_export_rtf_runs(const RtfRun *runs, int n){
    if (!runs || n <= 0) return strdup("no runs to export");
    char *out = rtf_write(runs, n);
    if (!out) return strdup("RTF write failed");
    FILE *f = fopen("/tmp/wubuos_export_runs.rtf", "wb");
    if (!f){ free(out); return strdup("open output failed"); }
    fputs(out, f); fclose(f); free(out);
    return strdup("RTF runs written: /tmp/wubuos_export_runs.rtf");
}

/* doccmd_redact_doc -- redact ranges of the model's plain-text dump. Used by
 * Privacy / Export-Marked-Document. Walks the doc tree and concatenates run
 * text into a buffer, then applies ranges. Returns strdup'd status. */
char *doccmd_redact_doc(wubumodel_doc *doc, const size_t *ranges, int n_ranges){
    if (!doc) return strdup("no model doc to redact");
    wubumodel_node *root = wubumodel_doc_root(doc);
    if (!root) return strdup("doc has no root");
    size_t off = 0, cap = 256;
    char *plain = (char*)malloc(cap);
    if (!plain) return strdup("oom");
    plain[0] = 0;
    /* DFS walk, collect WUBUMODEL_RUN text into `plain`. */
    wubumodel_node *stack[256]; int sp = 0;
    stack[sp++] = root;
    while (sp > 0){
        wubumodel_node *n = stack[--sp];
        for (wubumodel_node *c = wubumodel_node_first_child(n); c;
             c = wubumodel_node_next_sibling(c)){
            int k = wubumodel_node_kind(c);
            const char *txt = wubumodel_node_text(c);
            if (k == WUBUMODEL_RUN && txt){
                size_t tlen = strlen(txt);
                size_t need = off + tlen + 1;
                if (need > cap){ while (cap < need) cap *= 2;
                    char *nb = realloc(plain, cap);
                    if (!nb){ free(plain); return strdup("oom"); }
                    plain = nb;
                }
                memcpy(plain + off, txt, tlen); off += tlen;
            }
            if (sp < 255) stack[sp++] = c;
        }
    }
    plain[off] = 0;
    if (!ranges || n_ranges <= 0){
        free(plain);
        return strdup("no redact ranges given");
    }
    Redact *rd = redact_create();
    if (!rd){ free(plain); return strdup("redact create failed"); }
    for (int i = 0; i < n_ranges; i++){
        if (!redact_mark(rd, ranges[i*2], ranges[i*2 + 1])){
            redact_destroy(rd); free(plain);
            return strdup("redact_mark failed (out of bounds?)");
        }
    }
    char *out = redact_apply(rd, plain);
    redact_destroy(rd); free(plain);
    if (!out) return strdup("redact_apply failed");
    FILE *f = fopen("/tmp/wubuos_redacted.txt", "wb");
    if (!f){ free(out); return strdup("open output failed"); }
    fputs(out, f); fclose(f); free(out);
    return strdup("Redacted text written: /tmp/wubuos_redacted.txt");
}

/* doccmd_col_demo -- append/resolve a thread on the comment store. Used to
 * exercise the wubucol engine from the document shell. Returns status. */
char *doccmd_col_demo(void){
    Col *c = col_create();
    if (!c) return strdup("col_create failed");
    int tid = col_add(c, "doc:root", "user", "first comment on root");
    if (tid < 0){ col_destroy(c); return strdup("col_add failed"); }
    col_reply(c, tid, "reviewer", "agreed");
    col_resolve(c, tid, 1);
    int n = col_thread_count(c);
    char b[128];
    snprintf(b, sizeof b, "col ok: threads=%d resolved=%d",
             n, col_resolved(c, tid));
    col_destroy(c);
    return strdup(b);
}
