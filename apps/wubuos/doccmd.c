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
#include "cite.h"      /* citation store (DOC-68) */
#include "caption.h"   /* caption side-table (DOC-70) */
#include "heading.h"   /* semantic heading-level enforcement (UXA-49/50) */
#include "eqnum.h"     /* equation numbering (DOC-69) */
#include "vars.h"      /* ${name} variable expansion (DOC-73) */
#include "hash.h"      /* SHA-256 (FIPS 180-4) */
#include "sig.h"       /* HMAC document signature (EXP-90) */
#include "crdt.h"      /* node-sequence CRDT (LWW-merge) */
#include "csv.h"       /* RFC-4180 CSV parser (EXP-86) */
#include "focus.h"     /* focus indicator config (UXA-51) */
#include "watermark.h" /* page watermark config (DOC-71) */
#include "dyslexia.h"  /* dyslexia-friendly mode (UXA-52) */
#include "fmtpaint.h"  /* format painter (DOC-74) */
#include "sandbox.h"   /* plugin capability sandbox (SCR-100) */
#include "form.h"      /* form fields (DOC-72) */
#include "history.h"   /* revision history (DOC-75) */
#include "lang.h"      /* per-node language tag (DOC-76) */
#include "nesttab.h"   /* nested tables (DOC-77) */
#include "pdfextract.h"/* PDF text extraction (EXP-91) */
#include "pdfform.h"   /* PDF form writer (EXP-92, links wubuform) */
#include "scope.h"     /* scope labels per table (DOC-78) */
#include "sync.h"      /* sync store for CRDT replicas (DOC-79) */
#include "xps.h"       /* XPS writer (EXP-93) */
#include "aislot.h"    /* offline AI assist hook (SCR-99) */
#include "script.h"    /* wubuscript: script_eval */
#include "ublayout.h"  /* wubulayout_create (central pipeline -> export) */
#include "wuos_font.h" /* wuos_font_text_width / _height for measure callback */
#include "model.h"     /* wubumodel_doc_root */
#include "wubusvg/rast.h"  /* svg_rasterize_cb */
#include "qr.h"        /* wubuocr: qr_encode */

/* Spreadsheet & document analysis modules (2026-08-11 wave) */
#include "wubusort.h"
#include "wubufilter.h"
#include "wubusubtotal.h"
#include "wubugoalseek.h"
#include "wubusolver.h"
#include "wubupivot.h"
#include "wubuscenario.h"
#include "wubufreeze.h"
#include "wubuhyperlink.h"
#include "wubuthesaurus.h"
#include "wubugrammar.h"
#include "wubuindex.h"
#include "wubumailmerge.h"
#include "wubudiff.h"
#include "wubumasterdoc.h"
#include "wubudropcap.h"
#include "wuburuler.h"
#include "wubugridline.h"
#include "wubuicon.h"
#include "wubugallery.h"
#include "wubusidebar.h"
#include "wubutransition.h"
#include "wubuanimation.h"
#include "wubumasterslide.h"
#include "wubuconnector.h"
#include "wubuencrypt.h"
#include "wubumailexport.h"
#include "wubunotebookbar.h"
#include "wubuqr.h"
#include "wubusmartart.h"
#include "wububasic.h"
#include "wubu3d.h"

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

static wubulayout_doc *doccmd_layout(wubumodel_doc *doc); /* fwd (def below) */

char *doccmd_save(wubumodel_doc *doc, const char *path){
    if (!doc) return strdup("no model doc to save");
    char out[512];
    if (path && (strstr(path,".docx")||strstr(path,".docm")||strstr(path,".dotx")))
        snprintf(out,sizeof out,"%s", path);
    else if (path && (strstr(path,".odt")||strstr(path,".fodt")))
        snprintf(out,sizeof out,"%s", path);
    else if (path && strstr(path,".rtf"))
        snprintf(out,sizeof out,"%s", path);
    else if (path && (strstr(path,".epub")||strstr(path,".epub3")))
        snprintf(out,sizeof out,"%s", path);
    else if (path)
        snprintf(out,sizeof out,"%s.docx", path);
    else
        snprintf(out,sizeof out,"/tmp/wubuos_document.docx");
    int rc;
    const char *lab;
    if (strstr(out,".odt")||strstr(out,".fodt")){
        rc = wubumodel_write_odt(doc, out); lab = "ODT";
    } else if (strstr(out,".rtf")){
        /* save back to the loaded RTF format (round-trip, not a .docx sidecar) */
        wubulayout_doc *L = doccmd_layout(doc);
        rc = L ? wubuexp_rtf(L, out) : -1;
        if (L) wubulayout_destroy(L);
        lab = "RTF";
    } else if (strstr(out,".epub")||strstr(out,".epub3")){
        rc = epub_write(doc, out, "WuBuOffice Document", "en"); lab = "EPUB";
    } else {
        rc = wubumodel_write_docx(doc, out); lab = "DOCX";
    }
    if (rc==0){
        char b[768];
        snprintf(b,sizeof b,"%s saved: %s", lab, out);
        return strdup(b);
    }
    return strdup("save failed for that format");
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

/* doccmd_cite_demo -- add a citation entry and render inline + bibliography
 * text. Exercises src/wubucite (DOC-68). Writes bibliography to /tmp/wubuos_bib.txt. */
char *doccmd_cite_demo(void){
    Cite *c = cite_create();
    if (!c) return strdup("cite_create failed");
    cite_add(c, "smith2020", "article", "On CRDTs", "Alice Smith; Bob Jones", 2020);
    cite_add(c, "doe2019", "book", "Distributed Systems", "Jane Doe", 2019);
    char *inline1 = cite_inline(c, "smith2020");
    char *bib = cite_bibliography(c);
    int n = cite_count(c);
    char b[256];
    snprintf(b, sizeof b, "cite ok: count=%d inline1=%s bib_len=%zu",
             n, inline1 ? inline1 : "(null)", bib ? strlen(bib) : 0);
    if (bib){
        FILE *f = fopen("/tmp/wubuos_bib.txt", "wb");
        if (f){ fputs(bib, f); fclose(f); }
    }
    free(inline1); free(bib); cite_destroy(c);
    return strdup(b);
}

/* doccmd_caption_demo -- bind captions to (synthetic) node ids. Exercises
 * src/wubucaption (DOC-70). */
char *doccmd_caption_demo(void){
    CaptionMap *m = caption_create();
    if (!m) return strdup("caption_create failed");
    caption_set(m, 1001, "Figure 1: System architecture");
    caption_set(m, 1002, "Figure 2: CRDT merge diagram");
    const char *c2 = caption_get(m, 1002);
    char b[256];
    snprintf(b, sizeof b, "caption ok: count=%d node1002=%s",
             caption_count(m), c2 ? c2 : "(null)");
    caption_destroy(m);
    return strdup(b);
}

/* doccmd_heading_enforce -- walk the doc's SECTION nodes and assign sequential
 * levels. Exercises src/wubuheading (UXA-49/50). */
char *doccmd_heading_enforce(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    Heading *h = heading_create();
    if (!h) return strdup("heading_create failed");
    int n = heading_enforce(h, doc);
    char b[128];
    snprintf(b, sizeof b, "heading enforce ok: counted=%d", n);
    heading_destroy(h);
    return strdup(b);
}

/* doccmd_eqnum_scan -- number every equation-bearing FIELD node. Exercises
 * src/wubueqnum (DOC-69). */
char *doccmd_eqnum_scan(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    EqNum *e = eqnum_create();
    if (!e) return strdup("eqnum_create failed");
    int n = eqnum_scan(e, doc);
    char b[128];
    snprintf(b, sizeof b, "eqnum scan ok: numbered=%d", n);
    eqnum_destroy(e);
    return strdup(b);
}

/* doccmd_vars_expand -- set 2 variables and expand a template. Exercises
 * src/wubuvars (DOC-73). */
char *doccmd_vars_expand(const char *tpl){
    Vars *v = vars_create();
    if (!v) return strdup("vars_create failed");
    vars_set(v, "name", "Hermes");
    vars_set(v, "year", "2026");
    const char *text = tpl ? tpl : "Hello, ${name}! Year ${year}.";
    char *out = vars_expand(v, text);
    char b[256];
    snprintf(b, sizeof b, "vars expand: count=%d out=%s",
             vars_count(v), out ? out : "(null)");
    free(out); vars_destroy(v);
    return strdup(b);
}

/* doccmd_hash_sha256 -- one-shot SHA-256 of a string, hex output. Exercises
 * src/wubuhash (FIPS 180-4). */
char *doccmd_hash_sha256(const char *text){
    if (!text) return strdup("no text");
    uint8_t dig[WUBUHASH_SHA256_SIZE];
    char hex[WUBUHASH_SHA256_SIZE * 2 + 1];
    sha256(text, strlen(text), dig);
    hash_hex(dig, hex);
    char b[160];
    snprintf(b, sizeof b, "sha256: %s", hex);
    return strdup(b);
}

/* doccmd_doc_sign -- HMAC-SHA256 sign a fixed message with a fixed key.
 * Exercises src/wubusig (EXP-90) which links against wubuhash. */
char *doccmd_doc_sign(void){
    const char *msg = "WuBuOffice document body";
    const char *key = "wubu-test-key-2026";
    uint8_t sig[32];
    char hex[65];
    sig_sign(msg, strlen(msg), key, strlen(key), sig);
    sig_hex(sig, hex);
    int ok = sig_verify(msg, strlen(msg), key, strlen(key), sig);
    char b[128];
    snprintf(b, sizeof b, "doc sig: ok=%d sig=%s...", ok, hex);
    return strdup(b);
}

/* doccmd_crdt_demo -- exercise a CRDT replica: insert 3 items, delete 1,
 * merge a peer replica with a concurrent insert. Exercises src/wubucrdt. */
char *doccmd_crdt_demo(void){
    Crdt *a = crdt_create("site-a");
    Crdt *b = crdt_create("site-b");
    if (!a || !b){ crdt_destroy(a); crdt_destroy(b); return strdup("crdt_create failed"); }
    crdt_insert(a, 0, "alpha");
    crdt_insert(a, 1, "beta");
    crdt_insert(a, 2, "gamma");
    crdt_insert(b, 0, "delta");     /* concurrent */
    crdt_merge(a, b);
    crdt_delete(a, 0);               /* delete "alpha" */
    char b2[160];
    snprintf(b2, sizeof b2, "crdt ok: count=%d first=%s",
             crdt_count(a), crdt_count(a) > 0 ? crdt_get(a, 0) : "(empty)");
    crdt_destroy(a); crdt_destroy(b);
    return strdup(b2);
}

/* doccmd_csv_parse -- parse a small RFC-4180 CSV with quoted fields. Exercises
 * src/wubucsv (EXP-86). */
char *doccmd_csv_parse(void){
    Csv *c = csv_create();
    if (!c) return strdup("csv_create failed");
    const char *txt = "name,age,city\n\"Alice, A.\",30,\"New York\"\nBob,25,LA\n";
    int ok = csv_parse(c, txt);
    char b[160];
    snprintf(b, sizeof b, "csv parse: ok=%d rows=%d cols=%d cell00=%s",
             ok, csv_rows(c), csv_cols(c),
             csv_rows(c) > 0 ? csv_cell(c, 0, 0) : "(none)");
    csv_destroy(c);
    return strdup(b);
}

/* doccmd_focus_demo -- configure a focus indicator. Exercises src/wubufocus
 * (UXA-51). */
char *doccmd_focus_demo(void){
    Focus *f = focus_create();
    if (!f) return strdup("focus_create failed");
    focus_set_enabled(f, 1);
    focus_set_width(f, 3);
    focus_set_color(f, 0x33, 0x99, 0xFF, 0xFF);
    char b[128];
    snprintf(b, sizeof b, "focus ok: en=%d w=%d color=0x%08x",
             focus_enabled(f), focus_width(f), focus_color(f));
    focus_destroy(f);
    return strdup(b);
}

/* doccmd_watermark_demo -- configure a watermark. Exercises src/wubuwatermark
 * (DOC-71). */
char *doccmd_watermark_demo(void){
    Watermark *w = watermark_create();
    if (!w) return strdup("watermark_create failed");
    watermark_set_text(w, "DRAFT");
    watermark_set_angle(w, -30);
    watermark_set_opacity(w, 0.15f);
    watermark_set_enabled(w, 1);
    char b[160];
    snprintf(b, sizeof b, "watermark ok: en=%d text=%s angle=%d op=%.2f",
             watermark_enabled(w), watermark_text(w),
             watermark_angle(w), watermark_opacity(w));
    watermark_destroy(w);
    return strdup(b);
}

/* doccmd_dyslexia_demo -- configure dyslexia-friendly mode. Exercises
 * src/wubudyslexia (UXA-52). */
char *doccmd_dyslexia_demo(void){
    Dyslexia *d = dyslexia_create();
    if (!d) return strdup("dyslexia_create failed");
    dyslexia_set_enabled(d, 1);
    dyslexia_set_face(d, "OpenDyslexic");
    dyslexia_set_spacing(d, 1.3f);
    char b[160];
    snprintf(b, sizeof b, "dyslexia ok: en=%d face=%s spacing=%.2f",
             dyslexia_enabled(d), dyslexia_face(d), dyslexia_spacing(d));
    dyslexia_destroy(d);
    return strdup(b);
}

/* doccmd_fmtpaint_demo -- pick format from a node, apply to another. Exercises
 * src/wubufmtpaint (DOC-74). */
char *doccmd_fmtpaint_demo(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    FmtPaint *f = fmtpaint_create();
    if (!f) return strdup("fmtpaint_create failed");
    /* Use the doc's root as both src and dst for the demo. */
    void *root = wubumodel_doc_root(doc);
    int picked = root ? fmtpaint_pick(f, root) : 0;
    int applied = root ? fmtpaint_apply(f, root) : -1;
    char b[128];
    snprintf(b, sizeof b, "fmtpaint ok: picked=%d applied=%d loaded=%d",
             picked, applied, fmtpaint_loaded(f));
    fmtpaint_destroy(f);
    return strdup(b);
}

/* doccmd_sandbox_demo -- register a plugin, grant subset of caps, exercise
 * allow/deny checks. Exercises src/wubusandbox (SCR-100). */
char *doccmd_sandbox_demo(void){
    Sandbox *s = sandbox_create();
    if (!s) return strdup("sandbox_create failed");
    int pid = sandbox_register(s, "test-plugin",
                               SBX_READ_DOC | SBX_WRITE_DOC | SBX_FS);
    if (pid < 0){ sandbox_destroy(s); return strdup("sandbox_register failed"); }
    /* Grant only read+fs, NOT write. */
    sandbox_grant(s, pid, SBX_READ_DOC | SBX_FS);
    int read_ok  = sandbox_check(s, pid, SBX_READ_DOC);
    int write_ok = sandbox_check(s, pid, SBX_WRITE_DOC);   /* should be denied */
    int fs_ok    = sandbox_check(s, pid, SBX_FS);
    int net_ok   = sandbox_check(s, pid, SBX_NET);         /* not requested */
    char b[200];
    snprintf(b, sizeof b, "sandbox ok: read=%d write=%d fs=%d net=%d denials=%d eff=0x%x",
             read_ok, write_ok, fs_ok, net_ok,
             sandbox_denials(s, pid), sandbox_effective(s, pid));
    sandbox_destroy(s);
    return strdup(b);
}

/* doccmd_form_demo -- add 3 form fields, set/get a value. Exercises
 * src/wubuform (DOC-72). */
char *doccmd_form_demo(void){
    Form *f = form_create();
    if (!f) return strdup("form_create failed");
    form_add(f, "name", FORM_TEXT, "Alice");
    form_add(f, "subscribe", FORM_CHECKBOX, "true");
    form_add(f, "plan", FORM_CHOICE, "pro");
    form_set_value(f, "name", "Bob");
    const char *v = form_value(f, "name");
    char b[160];
    snprintf(b, sizeof b, "form ok: count=%d name=%s",
             form_count(f), v ? v : "(null)");
    form_destroy(f);
    return strdup(b);
}

/* doccmd_history_demo -- commit 2 revisions, diff them. Exercises
 * src/wubuhistory (DOC-75). */
char *doccmd_history_demo(void){
    History *h = history_create();
    if (!h) return strdup("history_create failed");
    history_commit(h, "v1 text", 7, "user", "draft 1");
    history_commit(h, "v2 text", 7, "user", "draft 2");
    int n = history_count(h);
    char *diff = history_diff(h, 1, 2);
    char b[160];
    snprintf(b, sizeof b, "history ok: count=%d diff=%s",
             n, diff ? diff : "(null)");
    free(diff); history_destroy(h);
    return strdup(b);
}

/* doccmd_lang_demo -- set/get a language tag on a node id. Exercises
 * src/wubulang (DOC-76). */
char *doccmd_lang_demo(void){
    LangMap *m = lang_create();
    if (!m) return strdup("lang_create failed");
    lang_set(m, 5001, "en-US");
    lang_set(m, 5002, "fr-FR");
    const char *t1 = lang_get(m, 5001);
    const char *t2 = lang_get(m, 5002);
    char b[160];
    snprintf(b, sizeof b, "lang ok: count=%d node5001=%s node5002=%s",
             lang_count(m), t1 ? t1 : "(null)", t2 ? t2 : "(null)");
    lang_destroy(m);
    return strdup(b);
}

/* doccmd_nesttab_demo -- build + nest a table, check depth. Exercises
 * src/wubunesttab (DOC-77). Requires a wubumodel_doc. */
char *doccmd_nesttab_demo(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    void *root = wubumodel_doc_root(doc);
    if (!root) return strdup("no root");
    void *t1 = nesttab_build(doc, root, 2, 2);
    if (!t1) return strdup("nesttab_build failed");
    void *cell = nesttab_cell(t1, 0, 0, 2);
    if (!cell) return strdup("nesttab_cell failed");
    void *t2 = nesttab_nest(doc, cell, 2, 3);
    int depth = nesttab_depth(t2);
    int valid = nesttab_validate(t2);
    char b[128];
    snprintf(b, sizeof b, "nesttab ok: depth=%d valid=%d", depth, valid);
    return strdup(b);
}

/* doccmd_pdfextract_demo -- extract text from a tiny PDF. Exercises
 * src/wubupdfextract (EXP-91). */
char *doccmd_pdfextract_demo(void){
    /* A minimal valid PDF with a text "Hi" in stream. */
    const char *pdf =
        "%PDF-1.0\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
        "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
        "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] "
        "/Contents 4 0 R >>\nendobj\n"
        "4 0 obj\n<< /Length 20 >>\nstream\nBT /F1 12 Tf (Hi) Tj ET\nendstream\nendobj\n"
        "xref\n0 5\n0000000000 65535 f \ntrailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n0\n%%EOF\n";
    char *text = pdfextract_bytes((const uint8_t*)pdf, strlen(pdf));
    char b[160];
    snprintf(b, sizeof b, "pdfextract ok: text=%s",
             text ? text : "(null)");
    free(text);
    return strdup(b);
}

/* doccmd_pdfform_demo -- build a PDF form from a Form, write to /tmp. Exercises
 * src/wubupdfform (EXP-92). */
char *doccmd_pdfform_demo(void){
    Form *f = form_create();
    if (!f) return strdup("form_create failed");
    form_add(f, "username", FORM_TEXT, "");
    form_add(f, "agree", FORM_CHECKBOX, "true");
    int rc = pdfform_write_file(f, "/tmp/wubuos_form.pdf");
    char b[128];
    snprintf(b, sizeof b, "pdfform ok: rc=%d path=/tmp/wubuos_form.pdf", rc);
    form_destroy(f);
    return strdup(b);
}

/* doccmd_scope_demo -- set/get scope labels on table ids. Exercises
 * src/wubuscope (DOC-78). */
char *doccmd_scope_demo(void){
    ScopeMap *m = scope_create();
    if (!m) return strdup("scope_create failed");
    scope_set(m, 7001, "Sheet1.A1:C10");
    scope_set(m, 7002, "Sheet2.D1:F20");
    const char *s1 = scope_get(m, 7001);
    char b[160];
    snprintf(b, sizeof b, "scope ok: count=%d table7001=%s",
             scope_count(m), s1 ? s1 : "(null)");
    scope_destroy(m);
    return strdup(b);
}

/* doccmd_sync_demo -- open a /tmp sync store, put+get a blob. Exercises
 * src/wubusync (DOC-79). */
char *doccmd_sync_demo(void){
    Sync *s = sync_open("/tmp/wubuos_sync_test");
    if (!s) return strdup("sync_open failed");
    const char *blob = "replica-v1-data";
    sync_put(s, "doc:hash123", blob, strlen(blob), "site-alpha");
    char *got = NULL;
    size_t gotlen = 0;
    int found = sync_get(s, "doc:hash123", &got, &gotlen);
    char b[128];
    snprintf(b, sizeof b, "sync ok: found=%d len=%zu", found, gotlen);
    free(got); sync_close(s);
    return strdup(b);
}

/* doccmd_xps_demo -- write a tiny XPS file to /tmp. Exercises src/wubuxps
 * (EXP-93). */
char *doccmd_xps_demo(void){
    int rc = xps_write_file("/tmp/wubuos_export.xps", "Hello XPS", 200, 200);
    char b[128];
    snprintf(b, sizeof b, "xps ok: rc=%d path=/tmp/wubuos_export.xps", rc);
    return strdup(b);
}

/* doccmd_aislot_demo -- run the built-in (rule-based) AI summarizer. Exercises
 * src/wubuaislot (SCR-99). */
char *doccmd_aislot_demo(void){
    AiSlot *s = aislot_create();
    if (!s) return strdup("aislot_create failed");
    aislot_set_provider(s, NULL, NULL);  /* use built-in fallback */
    char *out = aislot_run(s, "summarize", "This is a long document about CRDTs and real-time collaboration.");
    char b[256];
    snprintf(b, sizeof b, "aislot ok: custom=%d out=%s",
             aislot_has_custom_provider(s), out ? out : "(null)");
    free(out); aislot_destroy(s);
    return strdup(b);
}

/* ---- Spreadsheet & document analysis modules (2026-08-11 wave) ---- */

/* shared cell accessor for string-array rows (sort/filter) */
static const char *doccmd_scell(void *row, int col, void *ud){ (void)col;(void)ud; return *(const char**)row; }

/* doccmd_sort_demo -- sort rows by a column. src/wubusort */
char *doccmd_sort_demo(void){
    char *a = strdup("Zoe"), *b = strdup("ann"), *c = strdup("Bob"), *d = strdup("Ann");
    void *rows[4] = { &a, &b, &c, &d };
    wubusort_col col = {0,0,0};
    int rc = wubusort_rows(rows, 4, &col, 1, doccmd_scell, NULL);
    char bf[160];
    snprintf(bf, sizeof bf, "sort ok: rc=%d first=%s", rc, *(const char**)rows[0]);
    free(a); free(b); free(c); free(d);
    return strdup(bf);
}

/* doccmd_filter_demo -- autofilter a small sheet. src/wubufilter */
char *doccmd_filter_demo(void){
    char *r0=strdup("10"),*r1=strdup("5"),*r2=strdup("30"),*r3=strdup("7"),*r4=strdup("15");
    void *rows[5] = {&r0,&r1,&r2,&r3,&r4};
    wubufilter_crit crit = {0, WUBUFILTER_GT, "6"};
    size_t out[5], n = 0;
    int rc = wubufilter_apply(rows, 5, &crit, 1, doccmd_scell, NULL, out, &n);
    char bf[128];
    snprintf(bf, sizeof bf, "filter ok: rc=%d matches=%zu", rc, n);
    free(r0);free(r1);free(r2);free(r3);free(r4);
    return strdup(bf);
}

/* subtotal row + accessors */
typedef struct { const char *k; const char *v; } SubRow;
static const char *doccmd_sub_key(void *row, void *ud){ (void)ud; return ((SubRow*)row)->k; }
static const char *doccmd_sub_val(void *row, void *ud){ (void)ud; return ((SubRow*)row)->v; }
char *doccmd_subtotal_demo(void){
    SubRow s0={"A","10"},s1={"B","5"},s2={"A","30"},s3={"B","7"};
    void *rows[4] = {&s0,&s1,&s2,&s3};
    wubusub_group *g=NULL; size_t n=0;
    int rc = wubusub_aggregate(rows, 4, doccmd_sub_key, doccmd_sub_val, WUBUSUB_SUM, NULL, &g, &n);
    char bf[128];
    snprintf(bf, sizeof bf, "subtotal ok: rc=%d groups=%zu", rc, n);
    wubusub_free(g, n);
    return strdup(bf);
}

/* goal-seek target f(x)=x^2-2 */
static double doccmd_seek_fn(double x, void *ud){ (void)ud; return x*x - 2.0; }
char *doccmd_goalseek_demo(void){
    wubugoalseek_result r;
    int rc = wubugoalseek(doccmd_seek_fn, 0.0, 0.0, 2.0, 1e-9, 1e-9, 32, NULL, &r);
    char bf[128];
    snprintf(bf, sizeof bf, "goalseek ok: rc=%d x=%.6f conv=%d", rc, r.x, r.converged);
    return strdup(bf);
}

/* solver: solve 2x+3y=8, 4x-y=2 -> x=1,y=2 */
char *doccmd_solver_demo(void){
    double A[4]={2,3,4,-1}, b[2]={8,2}, x[2];
    int rc = wubusolver_solve(A,2,b,x);
    char bf[128];
    snprintf(bf, sizeof bf, "solver ok: rc=%d x=%.2f y=%.2f", rc, x[0], x[1]);
    return strdup(bf);
}

/* pivot row + accessors */
typedef struct { const char *r; const char *c; const char *v; } PivRow;
static const char *doccmd_piv_r(void *row, void *ud){ (void)ud; return ((PivRow*)row)->r; }
static const char *doccmd_piv_c(void *row, void *ud){ (void)ud; return ((PivRow*)row)->c; }
static const char *doccmd_piv_v(void *row, void *ud){ (void)ud; return ((PivRow*)row)->v; }
char *doccmd_pivot_demo(void){
    PivRow p0={"East","Apples","10"},p1={"East","Pears","20"},p2={"West","Apples","5"};
    void *rows[3] = {&p0,&p1,&p2};
    wubupiv *t = wubupiv_build(rows,3,doccmd_piv_r,doccmd_piv_c,doccmd_piv_v,WUBUPIV_SUM,NULL);
    double v=0; int rc = t ? wubupiv_get(t,"East","Apples",&v) : -1;
    char bf[128];
    snprintf(bf, sizeof bf, "pivot ok: rows=%zu cols=%zu East/Apples=%.0f", t?t->nrows:0, t?t->ncols:0, v);
    wubupiv_free(t);
    return strdup(bf);
}

/* scenario */
char *doccmd_scenario_demo(void){
    wubuscenario *s = wubuscenario_create();
    wubuscen_cell cells[2] = { {0,0,"10"}, {1,1,"20"} };
    int rc = wubuscenario_set(s, "Pessimistic", cells, 2);
    size_t n = wubuscenario_count(s);
    wubuscenario_destroy(s);
    char bf[128];
    snprintf(bf, sizeof bf, "scenario ok: rc=%d scenarios=%zu", rc, n);
    return strdup(bf);
}

/* freeze panes */
char *doccmd_freeze_demo(void){
    wubufreeze f;
    int rc = wubufreeze_init(&f, 1, 1);
    int vr = wubufreeze_visible_row(&f, 5);
    char bf[128];
    snprintf(bf, sizeof bf, "freeze ok: rc=%d frozen_rows=%d vis_row5=%d", rc, wubufreeze_frozen_rows(&f), vr);
    return strdup(bf);
}

/* hyperlink */
char *doccmd_hyperlink_demo(void){
    wubuhyperlink *h = wubuhyperlink_create();
    int rc = wubuhyperlink_set(h, 1001, "https://example.com", "Example", "sec2");
    const wubuhyperlink_entry *e = wubuhyperlink_get(h, 1001);
    size_t n = wubuhyperlink_count(h);
    char target[96] = "(null)";
    if (e && e->target) { strncpy(target, e->target, sizeof target - 1); target[sizeof target - 1] = 0; }
    wubuhyperlink_destroy(h);
    char bf[160];
    snprintf(bf, sizeof bf, "hyperlink ok: rc=%d links=%zu target=%s", rc, n, target);
    return strdup(bf);
}

/* thesaurus */
char *doccmd_thesaurus_demo(void){
    wubuthesaurus *t = wubuthesaurus_create();
    const char *happy[] = {"glad","joyful","cheerful",NULL};
    int rc = wubuthesaurus_add(t,"happy",happy);
    const char **s = wubuthesaurus_lookup(t,"happy");
    size_t n = wubuthesaurus_count(t);
    char first[48] = "(null)";
    if (s && s[0]) { strncpy(first, s[0], sizeof first - 1); first[sizeof first - 1] = 0; }
    wubuthesaurus_destroy(t);
    char bf[128];
    snprintf(bf, sizeof bf, "thesaurus ok: rc=%d entries=%zu first=%s", rc, n, first);
    return strdup(bf);
}

/* grammar check */
char *doccmd_grammar_demo(void){
    wubugrammar_finding f[16];
    int n = wubugrammar_check("this is is alot of fun", f, 16);
    char bf[128];
    snprintf(bf, sizeof bf, "grammar ok: findings=%d first_id=%d", n, n?f[0].issue_id:-1);
    return strdup(bf);
}

/* index */
char *doccmd_index_demo(void){
    wubuindex *ix = wubuindex_create();
    wubuindex_add_term(ix,"Apple");
    wubuindex_add_term(ix,"Banana");
    int rc = wubuindex_feed_page(ix,"The Apple falls",1);
    const wubuindex_entry *e = wubuindex_get(ix,0);
    size_t n = wubuindex_count(ix);
    char term[48] = "(null)"; size_t pages = 0;
    if (e) { strncpy(term, e->term, sizeof term - 1); term[sizeof term - 1] = 0; pages = e->npages; }
    wubuindex_destroy(ix);
    char bf[128];
    snprintf(bf, sizeof bf, "index ok: rc=%d entries=%zu first=%s pages=%zu", rc, n, term, pages);
    return strdup(bf);
}

/* mail merge */
char *doccmd_mailmerge_demo(void){
    wubumailmerge *m = wubumailmerge_create();
    const wubumailmerge_field rec[] = {{"Name","Alice"},{"City","Boston"},{"Amount","100"},{NULL,NULL}};
    int rc = wubumailmerge_add_record(m, rec);
    char *out = wubumailmerge_merge(m, 0, "Dear ${Name} of {City}.");
    size_t n = wubumailmerge_record_count(m);
    wubumailmerge_destroy(m);
    char bf[160];
    snprintf(bf, sizeof bf, "mailmerge ok: rc=%d records=%zu merged=%s", rc, n, out?out:"(null)");
    free(out);
    return strdup(bf);
}

/* diff */
char *doccmd_diff_demo(void){
    int n = wubudiff_count("a\nb\nc\n","a\nc\n");
    char bf[96];
    snprintf(bf, sizeof bf, "diff ok: hunks=%d", n);
    return strdup(bf);
}

/* master doc */
char *doccmd_masterdoc_demo(void){
    wubumasterdoc *m = wubumasterdoc_create();
    int rc = wubumasterdoc_add(m,"ch1.docx");
    wubumasterdoc_add(m,"ch2.docx");
    const char *first = wubumasterdoc_get(m,0);
    size_t n = wubumasterdoc_count(m);
    char firstbuf[48] = "(null)";
    if (first) { strncpy(firstbuf, first, sizeof firstbuf - 1); firstbuf[sizeof firstbuf - 1] = 0; }
    wubumasterdoc_destroy(m);
    char bf[128];
    snprintf(bf, sizeof bf, "masterdoc ok: rc=%d subs=%zu first=%s", rc, n, firstbuf);
    return strdup(bf);
}

/* doccmd_dropcap_demo -- first-letter drop cap. src/wubudropcap */
char *doccmd_dropcap_demo(void){
    wubudropcap d;
    int rc = wubudropcap_init(&d, 3);
    int lines = wubudropcap_lines(&d);
    char bf[96];
    snprintf(bf, sizeof bf, "dropcap ok: rc=%d lines=%d", rc, lines);
    return strdup(bf);
}

/* doccmd_ruler_demo -- page ruler margins. src/wuburuler */
char *doccmd_ruler_demo(void){
    wuburuler r;
    int rc = wuburuler_init(&r, 612, 792);
    double w=0, h=0;
    wuburuler_content(&r, &w, &h);
    char bf[128];
    snprintf(bf, sizeof bf, "ruler ok: rc=%d content=%.0fx%.0f", rc, w, h);
    return strdup(bf);
}

/* doccmd_gridline_demo -- gridline toggle. src/wubugridline */
char *doccmd_gridline_demo(void){
    wubugridline g;
    int rc = wubugridline_init(&g);
    wubugridline_toggle(&g);
    char bf[96];
    snprintf(bf, sizeof bf, "gridline ok: rc=%d show=%d", rc, g.show);
    return strdup(bf);
}

/* doccmd_icon_demo -- icon registry. src/wubuicon */
char *doccmd_icon_demo(void){
    wubuicon *i = wubuicon_create();
    int rc = wubuicon_add(i,"save","M5 5h14v14H5z");
    size_t n = wubuicon_count(i);
    wubuicon_destroy(i);
    char bf[96];
    snprintf(bf, sizeof bf, "icon ok: rc=%d icons=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_gallery_demo -- gallery collections. src/wubugallery */
char *doccmd_gallery_demo(void){
    wubugallery *g = wubugallery_create();
    int rc = wubugallery_add_item(g,"Clipart","star.png");
    wubugallery_add_item(g,"Clipart","heart.png");
    size_t n = wubugallery_count(g);
    wubugallery_destroy(g);
    char bf[96];
    snprintf(bf, sizeof bf, "gallery ok: rc=%d galleries=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_sidebar_demo -- sidebar panels. src/wubusidebar */
char *doccmd_sidebar_demo(void){
    wubusidebar *s = wubusidebar_create();
    int rc = wubusidebar_add_panel(s,"Styles");
    wubusidebar_add_panel(s,"Navigator");
    size_t n = wubusidebar_count(s);
    wubusidebar_destroy(s);
    char bf[96];
    snprintf(bf, sizeof bf, "sidebar ok: rc=%d panels=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_transition_demo -- slide transition. src/wubutransition */
char *doccmd_transition_demo(void){
    wubutransition t;
    int rc = wubutransition_set(&t, WUBU_TR_FADE, 0.5, 1, 2.0);
    char bf[96];
    snprintf(bf, sizeof bf, "transition ok: rc=%d type=%d", rc, (int)t.type);
    return strdup(bf);
}

/* doccmd_animation_demo -- keyframe animation. src/wubuanimation */
char *doccmd_animation_demo(void){
    wubuanimation *a = wubuanimation_create();
    int rc = wubuanimation_add(a,"title",WUBU_AN_FADE,1.0,0.0,0);
    size_t n = wubuanimation_count(a);
    wubuanimation_destroy(a);
    char bf[96];
    snprintf(bf, sizeof bf, "animation ok: rc=%d keys=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_masterslide_demo -- master slide theme. src/wubumasterslide */
char *doccmd_masterslide_demo(void){
    wubumasterslide *m = wubumasterslide_create();
    int rc = wubumasterslide_set_theme(m,"1F3B8C",24.0);
    double fs = wubumasterslide_fontsize(m);
    wubumasterslide_destroy(m);
    char bf[96];
    snprintf(bf, sizeof bf, "masterslide ok: rc=%d font=%.0f", rc, fs);
    return strdup(bf);
}

/* doccmd_connector_demo -- diagram connector. src/wubuconnector */
char *doccmd_connector_demo(void){
    wubuconnector *c = wubuconnector_create();
    int rc = wubuconnector_add(c,"A","out","B","in");
    size_t n = wubuconnector_count(c);
    wubuconnector_destroy(c);
    char bf[96];
    snprintf(bf, sizeof bf, "connector ok: rc=%d edges=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_encrypt_demo -- password document encryption. src/wubuencrypt */
char *doccmd_encrypt_demo(void){
    const char *msg = "Classified WuBuOffice doc";
    size_t elen=0, dlen=0;
    unsigned char *enc = wubuencrypt_document("secret", (const unsigned char*)msg, strlen(msg), &elen);
    unsigned char *dec = enc ? wubuencrypt_open("secret", enc, elen, &dlen) : NULL;
    int ok = (dec && dlen == strlen(msg) && memcmp(dec, msg, dlen)==0) ? 1 : 0;
    free(enc); free(dec);
    char bf[96];
    snprintf(bf, sizeof bf, "encrypt ok: rc=%d roundtrip=%d", ok, ok);
    return strdup(bf);
}

/* doccmd_mailexport_demo -- RFC-5322 mail render. src/wubumailexport */
char *doccmd_mailexport_demo(void){
    wubumailexport m = {0};
    int rc = wubumailexport_build(&m,"b@x.com","a@y.com","Re","body");
    char *rendered = wubumailexport_render(&m);
    size_t rlen = rendered ? strlen(rendered) : 0;
    free(rendered);
    wubumailexport_free(&m);
    char bf[96];
    snprintf(bf, sizeof bf, "mailexport ok: rc=%d len=%zu", rc, rlen);
    return strdup(bf);
}

/* doccmd_notebookbar_demo -- sheet tab strip. src/wubunotebookbar */
char *doccmd_notebookbar_demo(void){
    wubunotebookbar *n = wubunotebookbar_create();
    int rc = wubunotebookbar_add(n,"Sheet1");
    wubunotebookbar_add(n,"Sheet2");
    size_t c = wubunotebookbar_count(n);
    wubunotebookbar_destroy(n);
    char bf[96];
    snprintf(bf, sizeof bf, "notebookbar ok: rc=%d tabs=%zu", rc, c);
    return strdup(bf);
}

/* doccmd_qr_demo -- QR render. src/wubuqr */
char *doccmd_qr_demo(void){
    int size = 0;
    char *qr = wubuqr_render_ascii("WUBU", &size);
    int ok = qr ? 1 : 0;
    free(qr);
    char bf[96];
    snprintf(bf, sizeof bf, "qr ok: ok=%d size=%d", ok, size);
    return strdup(bf);
}

/* doccmd_smartart_demo -- diagram layout. src/wubusmartart */
char *doccmd_smartart_demo(void){
    wubusmartart *s = wubusmartart_create();
    int rc = wubusmartart_set_layout(s, WUBU_SA_CYCLE);
    wubusmartart_add_node(s,"Plan");
    size_t n = wubusmartart_count(s);
    wubusmartart_destroy(s);
    char bf[96];
    snprintf(bf, sizeof bf, "smartart ok: rc=%d nodes=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_basic_demo -- minimal BASIC macro engine. src/wububasic */
char *doccmd_basic_demo(void){
    wububasic *b = wububasic_create();
    static char out[256]; out[0]=0;
    wububasic_set_output(b, NULL, NULL); /* stdout default; use a capture fn below */
    (void)out;
    int rc = wububasic_load(b, "s = 0\nFOR i = 1 TO 4\n s = s + i\nNEXT\nPRINT s\nEND\n");
    if (rc == 0) rc = wububasic_run(b);
    const char *val = wububasic_get_var(b, "s");
    char bf[96];
    snprintf(bf, sizeof bf, "basic ok: rc=%d s=%s", rc, val ? val : "?");
    wububasic_destroy(b);
    return strdup(bf);
}

/* doccmd_3d_demo -- 3D mesh model. src/wubu3d */
char *doccmd_3d_demo(void){
    wubu3d *m = wubu3d_create();
    int rc = wubu3d_make_cube(m);
    size_t v = wubu3d_vertex_count(m), f = wubu3d_face_count(m);
    wubu3d_destroy(m);
    char bf[96];
    snprintf(bf, sizeof bf, "3d ok: rc=%d verts=%zu faces=%zu", rc, v, f);
    return strdup(bf);
}

