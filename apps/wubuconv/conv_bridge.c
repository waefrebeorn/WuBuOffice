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
