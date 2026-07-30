/* conv_bridge.c -- cross-family model transforms for WuBuOffice convert.
 * See conv_bridge.h. Clean-room C11. */

#include "conv_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grow the dm_doc block array as needed (owned by this module's callers). */
#define DM_PUSH(d, blk) do { \
    if ((d)->n + 1 > (d)->cap) { (d)->cap = (d)->cap ? (d)->cap * 2 : 8; (d)->blocks = realloc((d)->blocks, (d)->cap * sizeof(*(d)->blocks)); } \
    (blk) = &((d)->blocks[(d)->n++]); memset((blk), 0, sizeof(*(blk))); \
} while (0)

/* ---------- SHEET -> TEXT ---------- */

void wubuconv_sheet_to_text(const wubucell_book *b, dm_doc *out) {
    memset(out, 0, sizeof *out);
    out->cap = 8; out->blocks = calloc(out->cap, sizeof *out->blocks);
    int ns = wubucell_sheet_count(b);
    for (int s = 1; s <= ns; s++) {
        int mc = 0, mr = 0; wubucell_sheet_dims(b, s, &mc, &mr);
        char title[256]; snprintf(title, sizeof title, "Sheet: %s", wubucell_sheet_name(b, s));
        dm_block *h; DM_PUSH(out, h); h->kind = DM_BLOCK_PARA; h->para.style = strdup("Heading1"); h->para.text = strdup(title);
        dm_block *t; DM_PUSH(out, t); t->kind = DM_BLOCK_TABLE; t->table.rows = mr; t->table.cols = mc;
        size_t ncells = (size_t)mr * mc; if (ncells == 0) ncells = 1;
        t->table.cells = calloc(ncells, sizeof(dm_para *));
        for (int r = 1; r <= mr; r++) {
            for (int c = 1; c <= mc; c++) {
                wubucell_ckind k; const char *txt = NULL; double num = 0, cached = 0;
                char buf[64];
                if (wubucell_get(b, s, c, r, &k, &txt, &num, &cached) == 0) {
                    const char *val = (k == WUBUCELL_STR) ? txt : buf;
                    if (k != WUBUCELL_STR) snprintf(buf, sizeof buf, "%g", (k == WUBUCELL_NUM) ? num : cached);
                    dm_para *cp = calloc(1, sizeof *cp);
                    cp->text = strdup(val ? val : "");
                    cp->bold = (r == 1);
                    t->table.cells[(size_t)(r - 1) * mc + (c - 1)] = cp;
                }
            }
        }
    }
}

/* ---------- SHOW -> TEXT ---------- */

void wubuconv_show_to_text(const wubushow_pres *p, dm_doc *out) {
    memset(out, 0, sizeof *out);
    out->cap = 8; out->blocks = calloc(out->cap, sizeof *out->blocks);
    int ns = wubushow_slide_count(p);
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(p, i, &title, &body);
        dm_block *h; DM_PUSH(out, h); h->kind = DM_BLOCK_PARA; h->para.style = strdup("Heading1"); h->para.text = strdup(title ? title : "");
        if (body && body[0]) {
            dm_block *pa; DM_PUSH(out, pa); pa->kind = DM_BLOCK_PARA; pa->para.text = strdup(body);
        }
    }
}

/* ---------- TEXT -> SHEET ---------- */

void wubuconv_text_to_sheet(const dm_doc *d, wubucell_book *b) {
    int sh = wubucell_sheet(b, "Sheet1");
    int r = 0;
    for (size_t i = 0; i < d->n; i++) {
        dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            r++;
            wubucell_cell_s(b, sh, 1, r, bl->para.text ? bl->para.text : "");
        } else {
            for (size_t tr = 0; tr < bl->table.rows; tr++) {
                r++;
                for (size_t c = 0; c < bl->table.cols; c++) {
                    size_t idx = tr * bl->table.cols + c;
                    dm_para *cc = (idx < bl->table.rows * bl->table.cols) ? bl->table.cells[idx] : NULL;
                    const char *v = (cc && cc->text) ? cc->text : "";
                    wubucell_cell_s(b, sh, (int)c + 1, r, v);
                }
            }
        }
    }
}

/* ---------- TEXT -> SHOW ---------- */

void wubuconv_text_to_show(const dm_doc *d, wubushow_pres *p) {
    char *title = strdup("Untitled");
    /* growable body buffer (no manual realloc aliasing) */
    char *body = NULL; size_t bcap = 0, bcur = 0;
    #define BODY_APPEND(src, len) do { \
        size_t need = bcur + (len); \
        if (need + 1 > bcap) { size_t nc = bcap ? bcap * 2 : 64; while (need + 1 > nc) nc *= 2; body = realloc(body, nc); bcap = nc; } \
        memcpy(body + bcur, (src), (len)); bcur += (len); body[bcur] = '\0'; \
    } while (0)
    for (size_t i = 0; i < d->n; i++) {
        dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            const char *txt = bl->para.text ? bl->para.text : "";
            int is_h = bl->para.style && (strncmp(bl->para.style, "Heading", 7) == 0 || strcmp(bl->para.style, "Title") == 0);
            if (is_h) {
                if (title) { wubushow_slide(p, title, body ? body : ""); }
                free(title); title = strdup(txt);
                free(body); body = NULL; bcur = 0; bcap = 0;
            } else {
                size_t L = strlen(txt);
                if (L) {
                    if (bcur) BODY_APPEND("\n", 1);
                    BODY_APPEND(txt, L);
                }
            }
        } else {
            /* table: emit as body lines */
            for (size_t r = 0; r < bl->table.rows; r++) {
                for (size_t c = 0; c < bl->table.cols; c++) {
                    dm_para *cc = bl->table.cells[r * bl->table.cols + c];
                    const char *v = (cc && cc->text) ? cc->text : "";
                    size_t L = strlen(v);
                    if (bcur) BODY_APPEND(" ", 1);
                    BODY_APPEND(v, L);
                }
                BODY_APPEND("\n", 1);
            }
        }
    }
    #undef BODY_APPEND
    if (title) { wubushow_slide(p, title, body ? body : ""); }
    free(title); free(body);
}

/* ---------- MODEL -> TEXT ---------- */

/* Collect all RUN text under a node (depth-first), into a growable buffer. */
static void model_collect_runs(const wubumodel_node *n,
                               char **buf, size_t *cap, size_t *len) {
    if (!n) return;
    /* if this node is itself a RUN, grab its text */
    if (wubumodel_node_kind(n) == WUBUMODEL_RUN) {
        const char *t = wubumodel_run_text(n);
        if (t) {
            size_t l = strlen(t);
            if (*buf == NULL || *len + l + 1 > *cap) {
                size_t nc = *cap ? *cap * 2 : 64;
                while (*len + l + 1 > nc) nc *= 2;
                *buf = realloc(*buf, nc); *cap = nc;
            }
            memcpy(*buf + *len, t, l); *len += l; (*buf)[*len] = '\0';
        }
    }
    /* also handle nodes that carry a direct text payload */
    const char *nt = wubumodel_node_text(n);
    if (nt && *nt) {
        size_t l = strlen(nt);
        if (*buf == NULL || *len + l + 1 > *cap) {
            size_t nc = *cap ? *cap * 2 : 64;
            while (*len + l + 1 > nc) nc *= 2;
            *buf = realloc(*buf, nc); *cap = nc;
        }
        memcpy(*buf + *len, nt, l); *len += l; (*buf)[*len] = '\0';
    }
    /* recurse into children */
    for (wubumodel_node *c = wubumodel_node_first_child(n); c;
         c = wubumodel_node_next_sibling(c))
        model_collect_runs(c, buf, cap, len);
}

void wubuconv_model_to_text(const wubumodel_doc *m, dm_doc *out) {
    memset(out, 0, sizeof *out);
    out->cap = 8; out->blocks = calloc(out->cap, sizeof *out->blocks);
    if (!out->blocks) return;

    /* Walk the top-level sections. Each section maps to one or more
     * dm_blocks: headings become styled paragraphs, paragraphs become
     * plain paragraphs, tables become dm_table blocks. */
    for (wubumodel_node *sec = wubumodel_doc_root(m); sec;
         sec = wubumodel_node_next_sibling(sec)) {
        wubumodel_kind k = wubumodel_node_kind(sec);

        if (k == WUBUMODEL_TABLE) {
            /* Direct table at section level: extract cells. */
            /* Count rows/cols by walking children. */
            size_t rows = 0, cols = 0;
            for (wubumodel_node *r = wubumodel_node_first_child(sec); r;
                 r = wubumodel_node_next_sibling(r)) {
                rows++;
                size_t rc = 0;
                for (wubumodel_node *cell = wubumodel_node_first_child(r); cell;
                     cell = wubumodel_node_next_sibling(cell)) {
                    rc++;
                    /* collect cell text */
                }
                if (rc > cols) cols = rc;
            }
            if (rows && cols) {
                dm_para **cells = calloc(rows * cols, sizeof(dm_para *));
                if (cells) {
                    size_t ri = 0;
                    for (wubumodel_node *r = wubumodel_node_first_child(sec); r;
                         r = wubumodel_node_next_sibling(r), ri++) {
                        size_t ci = 0;
                        for (wubumodel_node *cell = wubumodel_node_first_child(r); cell;
                             cell = wubumodel_node_next_sibling(cell), ci++) {
                            char buf[256]; size_t bcap = sizeof buf, blen = 0;
                            char *dyn = NULL;
                            model_collect_runs(cell, &dyn, &bcap, &blen);
                            const char *txt = dyn ? dyn : "";
                            if (!dyn) { strncpy(buf, txt, sizeof buf - 1); buf[sizeof buf - 1] = 0; txt = buf; }
                            dm_para *cp = calloc(1, sizeof *cp);
                            cp->text = strdup(txt ? txt : "");
                            cells[ri * cols + ci] = cp;
                            free(dyn);
                        }
                    }
                    dm_block *t; DM_PUSH(out, t);
                    t->kind = DM_BLOCK_TABLE;
                    t->table.rows = rows; t->table.cols = cols;
                    t->table.cells = cells;
                }
            }
            continue;
        }

        /* For non-table nodes, collect text and emit a paragraph. */
        char buf[1024]; size_t bcap = sizeof buf, blen = 0;
        char *dyn = NULL;
        model_collect_runs(sec, &dyn, &bcap, &blen);
        const char *txt = dyn ? dyn : "";
        if (dyn && blen >= sizeof buf) {
            /* dyn is the full text */
            txt = dyn;
        } else if (!dyn) {
            txt = "";
        }

        /* Determine if this is a heading (SECTION with a HEADER child, or
         * a node with a "Title"/"Heading" style). */
        const char *style = NULL;
        wubumodel_style *s = wubumodel_node_style(sec);
        if (s) {
            /* Check for heading-like style properties */
            const char *pval = wubumodel_style_get_prop(s, "pStyle");
            if (pval && strncmp(pval, "Heading", 7) == 0) style = pval;
            if (!style) {
                const char *sz = wubumodel_style_get_prop(s, "sz");
                if (sz && atoi(sz) >= 32) style = "Heading1";
            }
        }

        /* Also check children for heading structure */
        if (!style) {
            for (wubumodel_node *c = wubumodel_node_first_child(sec); c;
                 c = wubumodel_node_next_sibling(c)) {
                if (wubumodel_node_kind(c) == WUBUMODEL_HEADER) {
                    /* This section has a header — treat the section as a heading */
                    char hbuf[256]; size_t hcap = sizeof hbuf, hlen = 0;
                    char *hdyn = NULL;
                    model_collect_runs(c, &hdyn, &hcap, &hlen);
                    const char *htxt = hdyn ? hdyn : "";
                    dm_block *h; DM_PUSH(out, h);
                    h->kind = DM_BLOCK_PARA;
                    h->para.style = strdup("Heading1");
                    h->para.text = strdup(htxt);
                    free(hdyn);
                    /* Emit body paragraphs from non-header children */
                    for (wubumodel_node *c2 = wubumodel_node_first_child(sec); c2;
                         c2 = wubumodel_node_next_sibling(c2)) {
                        if (wubumodel_node_kind(c2) == WUBUMODEL_HEADER) continue;
                        if (wubumodel_node_kind(c2) == WUBUMODEL_PARAGRAPH ||
                            wubumodel_node_kind(c2) == WUBUMODEL_BLOCK) {
                            char b2[1024]; size_t b2cap = sizeof b2, b2len = 0;
                            char *b2dyn = NULL;
                            model_collect_runs(c2, &b2dyn, &b2cap, &b2len);
                            const char *b2txt = b2dyn ? b2dyn : "";
                            if (b2len > 0) {
                                dm_block *p; DM_PUSH(out, p);
                                p->kind = DM_BLOCK_PARA;
                                p->para.style = NULL;
                                p->para.bold = 0;
                                p->para.text = strdup(b2txt);
                            }
                            free(b2dyn);
                        }
                    }
                    free(dyn);
                    continue;
                }
            }
        }

        /* Plain paragraph */
        if (blen > 0 || !style) {
            dm_block *p; DM_PUSH(out, p);
            p->kind = DM_BLOCK_PARA;
            p->para.style = style ? strdup(style) : NULL;
            p->para.bold = 0;
            p->para.text = strdup(txt);
        }
        free(dyn);
    }
}
