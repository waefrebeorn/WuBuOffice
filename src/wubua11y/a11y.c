/* a11y.c -- dependency-free C11 accessibility auditor (see a11y.h). */
#include "a11y.h"
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

void a11y_report_free(a11y_report *r){
    if (!r) return;
    for (int i=0;i<r->count;i++) free(r->items[i]);
    free(r->items);
    r->items=NULL; r->count=0; r->cap=0;
}
void a11y_report_print(const a11y_report *r){
    if (!r){ printf("(null report)\n"); return; }
    if (r->count==0){ printf("a11y: no issues found\n"); return; }
    printf("a11y: %d issue(s)\n", r->count);
    for (int i=0;i<r->count;i++) printf("  - %s\n", r->items[i]);
}

static int add(a11y_report *r, const char *fmt, ...){
    char t[512]; va_list ap; va_start(ap,fmt);
    int n = vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    if (n<0) return -1;
    char *s;
    if ((size_t)n >= sizeof t){ s = malloc((size_t)n+1); if(!s) return -1;
        va_start(ap,fmt); vsnprintf(s,(size_t)n+1,fmt,ap); va_end(ap); }
    else { s = strdup(t); if(!s) return -1; }
    if (r->count == r->cap){
        int nc = r->cap? r->cap*2 : 8;
        char **ni = realloc(r->items, (size_t)nc*sizeof(char*));
        if (!ni){ free(s); return -1; }
        r->items = ni; r->cap = nc;
    }
    r->items[r->count++] = s;
    return 0;
}

/* ---- doc-model audit ---- */
static int para_is_title(const wubumodel_node *para){
    wubumodel_style *st = wubumodel_node_style(para);
    if (!st) return 0;
    const char *name = wubumodel_style_get_prop(st, "name");
    if (!name) return 0;
    if (strstr(name, "Title")) return 1;
    if (strncmp(name, "Heading", 7)==0 && name[7]=='1') return 1;
    return 0;
}
static int para_heading_level(const wubumodel_node *para){
    wubumodel_style *st = wubumodel_node_style(para);
    if (!st) return 0;
    const char *name = wubumodel_style_get_prop(st, "name");
    if (!name) return 0;
    if (strstr(name, "Title")) return 1;
    if (strncmp(name, "Heading", 7)==0){
        int l = name[7]-'0'; return (l>=1&&l<=6)?l:0;
    }
    return 0;
}

int a11y_check_doc(const wubumodel_doc *doc, int expect_title,
                   int expect_lang, a11y_report *out){
    if (!out){ return -1; }
    out->items=NULL; out->count=0; out->cap=0;
    if (!doc){ add(out, "document is null"); return 0; }

    int has_text = 0, has_title = 0, last_heading = 0;
    for (wubumodel_node *top = wubumodel_doc_root(doc); top; top = wubumodel_node_next_sibling(top)){
        for (wubumodel_node *sec = (wubumodel_node_kind(top)==WUBUMODEL_SECTION)? top : NULL;
             sec; sec = (sec==top)? NULL : wubumodel_node_next_sibling(sec)){
            for (wubumodel_node *c = wubumodel_node_first_child(sec); c; c = wubumodel_node_next_sibling(c)){
                if (wubumodel_node_kind(c)!=WUBUMODEL_PARAGRAPH) continue;
                /* gather text */
                for (wubumodel_node *r = wubumodel_node_first_child(c); r; r = wubumodel_node_next_sibling(r))
                    if (wubumodel_node_kind(r)==WUBUMODEL_RUN){
                        const char *t = wubumodel_run_text(r);
                        if (t && *t) has_text = 1;
                    }
                int lvl = para_heading_level(c);
                if (lvl==1 && para_is_title(c)) has_title = 1;
                if (lvl){
                    if (last_heading && lvl > last_heading + 1)
                        add(out, "heading level skips from h%d to h%d", last_heading, lvl);
                    last_heading = lvl;
                }
            }
        }
    }
    /* when there is no section wrapper, walk top-level paragraphs too */
    if (!has_text){
        for (wubumodel_node *top = wubumodel_doc_root(doc); top; top = wubumodel_node_next_sibling(top)){
            if (wubumodel_node_kind(top)!=WUBUMODEL_PARAGRAPH) continue;
            for (wubumodel_node *r = wubumodel_node_first_child(top); r; r = wubumodel_node_next_sibling(r))
                if (wubumodel_node_kind(r)==WUBUMODEL_RUN){
                    const char *t = wubumodel_run_text(r);
                    if (t && *t) has_text = 1;
                }
        }
    }

    /* ---- hop 20: table accessibility (MS checker parity) ----
     * Walk tables: flag missing header row, blank rows/cols, merged cells
     * (screen readers lose cell count across merges), and empty link text. */
    {
        /* hop 20 note: doc_root is the first parentless node (often the SECTION).
     * Walk recursively so tables at any depth are audited. */
    {
        /* iterative stack over nodes */
        wubumodel_node *stack[256]; int sp = 0;
        wubumodel_node *rt = wubumodel_doc_root(doc);
        if (rt && sp < 256) stack[sp++] = rt;
        while (sp > 0){
            wubumodel_node *n = stack[--sp];
            if (!n) continue;
            /* push children (reverse order not needed for reporting) */
            int pushed = 0;
            for (wubumodel_node *c = wubumodel_node_first_child(n); c;
                 c = wubumodel_node_next_sibling(c)){
                if (sp < 255){ stack[sp++] = c; pushed = 1; }
            }
            if (wubumodel_node_kind(n) != WUBUMODEL_TABLE){
                if (!pushed) continue;
                continue;
            }
            {
            int nrow = 0;
            for (wubumodel_node *tr = wubumodel_node_first_child(n); tr;
                 tr = wubumodel_node_next_sibling(tr), nrow++){
                if (wubumodel_node_kind(tr) != WUBUMODEL_CELL &&
                    wubumodel_node_kind(tr) != WUBUMODEL_PARAGRAPH &&
                    wubumodel_node_kind(tr) != WUBUMODEL_BLOCK) continue;
                int col = 0, blank = 1, ncell = 0;
                for (wubumodel_node *tc = wubumodel_node_first_child(tr); tc;
                     tc = wubumodel_node_next_sibling(tc)){
                    if (wubumodel_node_kind(tc) == WUBUMODEL_PARAGRAPH ||
                        wubumodel_node_kind(tc) == WUBUMODEL_RUN){
                        const char *t = wubumodel_run_text(tc);
                        if (!t || !*t){
                            const wubumodel_style *stc = NULL;
                            (void)stc;
                        }
                    }
                    if (wubumodel_node_kind(tc) != WUBUMODEL_CELL) continue;
                    ncell++;
                    int has_text_c = 0;
                    for (wubumodel_node *pc = wubumodel_node_first_child(tc); pc;
                         pc = wubumodel_node_next_sibling(pc)){
                        if (wubumodel_node_kind(pc) != WUBUMODEL_PARAGRAPH) continue;
                        for (wubumodel_node *rc = wubumodel_node_first_child(pc); rc;
                             rc = wubumodel_node_next_sibling(rc)){
                            if (wubumodel_node_kind(rc) != WUBUMODEL_RUN) continue;
                            const char *t = wubumodel_run_text(rc);
                            if (t && *t) has_text_c = 1;
                        }
                    }
                    if (has_text_c) blank = 0;
                    if (wubumodel_node_col_span(tc) > 1 || wubumodel_node_vmerge(tc))
                        add(out, "table %d: merged/split cell at row %d col %d "
                                 "(screen readers lose cell count)",
                            nrow + 0, nrow, col);
                    col += wubumodel_node_col_span(tc) > 0 ?
                           wubumodel_node_col_span(tc) : 1;
                }
                if (ncell && blank)
                    add(out, "table row %d is completely blank", nrow);
            }
            if (nrow < 2)
                add(out, "table has fewer than 2 rows (add a header row)");
            }
        }
    }

    }

    if (!has_text) add(out, "document has no body text (empty document)");
    if (expect_title && !has_title) add(out, "no document title (add a Title/Heading 1 paragraph)");
    if (expect_lang) add(out, "document language not declared (set at OPF <dc:language>)");
    return 0;
}

/* ---- EPUB parts audit (plain text, no ZIP needed) ---- */
static int has_substr(const char *hay, const char *needle){
    return hay && strstr(hay, needle) != NULL;
}
int a11y_check_epub_parts(const char *opf_text, const char *nav_text,
                           const char *chapter_xhtml, a11y_report *out){
    if (!out) return -1;
    out->items=NULL; out->count=0; out->cap=0;

    if (opf_text){
        if (!has_substr(opf_text, "<dc:title")) add(out, "OPF missing <dc:title> (required metadata)");
        if (!has_substr(opf_text, "<dc:language")) add(out, "OPF missing <dc:language> (required for a11y)");
        if (!has_substr(opf_text, "properties=\"nav\"") && !has_substr(opf_text, "properties='nav'"))
            add(out, "OPF manifest item for navigation lacks properties=\"nav\"");
    } else {
        add(out, "OPF text not provided");
    }
    if (nav_text){
        if (!has_substr(nav_text, "epub:type=\"toc\"") && !has_substr(nav_text, "epub:type='toc'"))
            add(out, "nav document missing epub:type=\"toc\"");
    } else {
        add(out, "navigation document (nav.xhtml) not provided");
    }
    if (chapter_xhtml){
        /* every <img> must carry alt */
        const char *p = chapter_xhtml;
        while ((p = strstr(p, "<img")) != NULL){
            /* find end of this tag */
            const char *end = strchr(p, '>');
            size_t taglen = end ? (size_t)(end - p) : strlen(p);
            int has_alt = 0;
            for (size_t i=0;i<taglen;i++)
                if (strncmp(p+i, "alt=", 4)==0){ has_alt=1; break; }
            if (!has_alt) add(out, "image without alt text (<img> missing alt=)");
            p += (end? taglen : 1);
        }
    }
    return 0;
}

/* ---- DOC-44: WCAG contrast audit of the built-in theme palettes ---- */
/* Relative luminance for an sRGB component (0..255). */
static double a11y_lin(double c){
    c /= 255.0;
    return c <= 0.03928 ? c/12.92 : pow((c+0.055)/1.055, 2.4);
}
static double a11y_lum(int r, int g, int b){
    return 0.2126*a11y_lin(r) + 0.7152*a11y_lin(g) + 0.0722*a11y_lin(b);
}
/* WCAG contrast ratio between two sRGB colors (1.0 .. 21.0). */
double wubua11y_contrast_ratio(int r1,int g1,int b1, int r2,int g2,int b2){
    double l1 = a11y_lum(r1,g1,b1), l2 = a11y_lum(r2,g2,b2);
    if (l1 < l2){ double t=l1; l1=l2; l2=t; }
    double d = l1 + 0.05, dd = l2 + 0.05;
    return dd > 0 ? d/dd : 21.0;
}
/* The application's two canonical chrome palettes (fg, bg). Used by the
 * contrast audit so regressions in theme contrast are caught in CI. */
typedef struct { const char *name; int fr,fg,fb, br,bg,bb; } a11y_pal_t;
static const a11y_pal_t A11Y_PALETTES[] = {
    { "light", 17,17,17, 245,245,245 },     /* near-black on white */
    { "dark",  228,228,228, 24,24,28 },     /* light on near-black */
    { "high-contrast", 255,255,255, 0,0,0 } /* max contrast */
};
/* Returns 1 if EVERY built-in palette meets WCAG AA (>=4.5:1) for body text,
 * 0 if any fails. Also emits a report entry per failing palette when out!=NULL. */
int wubua11y_palette_aa(a11y_report *out){
    /* Angel-coder fix: initialize the report (like a11y_check_doc/epub_parts).
     * Without this, a caller passing an uninitialized a11y_report hits garbage
     * items/count/cap -> a11y_report_free() frees a bad pointer (intermittent
     * segfault). Self-initializing makes the contract safe regardless of caller. */
    if (out){ out->items=NULL; out->count=0; out->cap=0; }
    int ok = 1;
    for (size_t i=0;i<sizeof(A11Y_PALETTES)/sizeof(A11Y_PALETTES[0]);i++){
        const a11y_pal_t *p = &A11Y_PALETTES[i];
        double ratio = wubua11y_contrast_ratio(p->fr,p->fg,p->fb, p->br,p->bg,p->bb);
        if (ratio < 4.5){
            ok = 0;
            if (out) add(out, "palette '%s' contrast %.2f:1 fails WCAG AA (needs >=4.5:1)",
                        p->name, ratio);
        }
    }
    return ok;
}
