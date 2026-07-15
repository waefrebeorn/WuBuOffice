/* layout.c -- recursive XY-cut page segmentation into reading-order blocks.
 *
 * Algorithm (per region, after trimming to its ink bounding box):
 *   1. Measure the widest internal whitespace gutter along each axis.
 *   2. If neither axis has a gutter wide enough, the region is a leaf block.
 *   3. Otherwise split along the axis with the wider qualifying gutter, at
 *      EVERY qualifying gutter, producing ordered sub-regions (left->right for
 *      a vertical cut, top->bottom for a horizontal cut), and recurse each.
 * "Widest gutter wins" is what lets a full-width headline (no vertical gutter
 * passes through it) be peeled off by a horizontal cut before columns are split
 * -- recovering correct reading order without any orientation guesswork.
 */
#include "layout.h"

#include <stdlib.h>
#include <string.h>

struct OcrLayout {
    OcrBlock *blk;
    size_t n, cap;
};

static int layout_push(OcrLayout *L, size_t x0, size_t y0, size_t x1, size_t y1) {
    if (L->n == L->cap) {
        size_t nc = L->cap ? L->cap * 2 : 16;
        OcrBlock *nb = realloc(L->blk, nc * sizeof *nb);
        if (!nb) return -1;
        L->blk = nb; L->cap = nc;
    }
    L->blk[L->n++] = (OcrBlock){ x0, y0, x1, y1 };
    return 0;
}

/* Trim [*x0,*x1) x [*y0,*y1) to the tight bounding box of ink. Returns 1 if
 * any ink remains, 0 if the region is entirely blank. */
static int trim_to_ink(const OcrBinary *b, size_t *x0, size_t *y0,
                       size_t *x1, size_t *y1) {
    size_t minx = *x1, miny = *y1, maxx = *x0, maxy = *y0;
    int any = 0;
    for (size_t y = *y0; y < *y1; y++) {
        for (size_t x = *x0; x < *x1; x++) {
            if (ocr_binary_ink(b, x, y)) {
                any = 1;
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
        }
    }
    if (!any) return 0;
    *x0 = minx; *y0 = miny; *x1 = maxx + 1; *y1 = maxy + 1;
    return 1;
}

/* Column has ink anywhere in [y0,y1)? */
static int col_has_ink(const OcrBinary *b, size_t x, size_t y0, size_t y1) {
    for (size_t y = y0; y < y1; y++) if (ocr_binary_ink(b, x, y)) return 1;
    return 0;
}
/* Row has ink anywhere in [x0,x1)? */
static int row_has_ink(const OcrBinary *b, size_t y, size_t x0, size_t x1) {
    for (size_t x = x0; x < x1; x++) if (ocr_binary_ink(b, x, y)) return 1;
    return 0;
}

/* Widest internal empty-column run within [x0,x1) (region already trimmed, so
 * both edges have ink). */
static size_t widest_v_gutter(const OcrBinary *b, size_t x0, size_t y0,
                              size_t x1, size_t y1) {
    size_t best = 0, run = 0;
    for (size_t x = x0; x < x1; x++) {
        if (!col_has_ink(b, x, y0, y1)) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
}
static size_t widest_h_gutter(const OcrBinary *b, size_t x0, size_t y0,
                              size_t x1, size_t y1) {
    size_t best = 0, run = 0;
    for (size_t y = y0; y < y1; y++) {
        if (!row_has_ink(b, y, x0, x1)) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
}

static void xy_cut(const OcrBinary *b, size_t x0, size_t y0, size_t x1, size_t y1,
                   const OcrLayoutParams *p, OcrLayout *L, int depth);

/* Split along the vertical axis at every empty-column run >= min_gutter_v,
 * recursing each ink segment left-to-right. Returns number of pieces produced. */
static size_t split_vertical(const OcrBinary *b, size_t x0, size_t y0,
                             size_t x1, size_t y1, const OcrLayoutParams *p,
                             OcrLayout *L, int depth) {
    size_t pieces = 0, seg_start = x0, run = 0, x = x0;
    int in_seg = 0;
    for (x = x0; x < x1; x++) {
        int ink = col_has_ink(b, x, y0, y1);
        if (ink) {
            if (!in_seg) { seg_start = x; in_seg = 1; }
            run = 0;
        } else {
            run++;
            if (in_seg && run >= p->min_gutter_v) {
                xy_cut(b, seg_start, y0, x - run + 1, y1, p, L, depth + 1);
                pieces++;
                in_seg = 0;
            }
        }
    }
    if (in_seg) { xy_cut(b, seg_start, y0, x1, y1, p, L, depth + 1); pieces++; }
    return pieces;
}

static size_t split_horizontal(const OcrBinary *b, size_t x0, size_t y0,
                               size_t x1, size_t y1, const OcrLayoutParams *p,
                               OcrLayout *L, int depth) {
    size_t pieces = 0, seg_start = y0, run = 0, y = y0;
    int in_seg = 0;
    for (y = y0; y < y1; y++) {
        int ink = row_has_ink(b, y, x0, x1);
        if (ink) {
            if (!in_seg) { seg_start = y; in_seg = 1; }
            run = 0;
        } else {
            run++;
            if (in_seg && run >= p->min_gutter_h) {
                xy_cut(b, x0, seg_start, x1, y - run + 1, p, L, depth + 1);
                pieces++;
                in_seg = 0;
            }
        }
    }
    if (in_seg) { xy_cut(b, x0, seg_start, x1, y1, p, L, depth + 1); pieces++; }
    return pieces;
}

#define OCR_MAX_DEPTH 64

static void xy_cut(const OcrBinary *b, size_t x0, size_t y0, size_t x1, size_t y1,
                   const OcrLayoutParams *p, OcrLayout *L, int depth) {
    if (x1 <= x0 || y1 <= y0) return;
    if (!trim_to_ink(b, &x0, &y0, &x1, &y1)) return;   /* blank region */
    size_t w = x1 - x0, h = y1 - y0;

    if (depth >= OCR_MAX_DEPTH || (w < p->min_block_w && h < p->min_block_h)) {
        layout_push(L, x0, y0, x1, y1);
        return;
    }

    size_t gv = widest_v_gutter(b, x0, y0, x1, y1);
    size_t gh = widest_h_gutter(b, x0, y0, x1, y1);
    int v_ok = (gv >= p->min_gutter_v);
    int h_ok = (gh >= p->min_gutter_h);

    if (!v_ok && !h_ok) { layout_push(L, x0, y0, x1, y1); return; }

    /* widest qualifying gutter wins; tie -> prefer vertical (column) split */
    size_t pieces;
    if (v_ok && (!h_ok || gv >= gh))
        pieces = split_vertical(b, x0, y0, x1, y1, p, L, depth);
    else
        pieces = split_horizontal(b, x0, y0, x1, y1, p, L, depth);

    /* A split that failed to actually divide would recurse forever; guard it. */
    if (pieces <= 1) {
        /* remove any leaf(s) the degenerate split may have pushed, re-emit one */
        /* (pieces==1 means the single child already handled it; nothing to do) */
        if (pieces == 0) layout_push(L, x0, y0, x1, y1);
    }
}

static void auto_params(const OcrBinary *b, OcrLayoutParams *p) {
    size_t w = ocr_binary_width(b), h = ocr_binary_height(b);
    p->min_gutter_v = w / 100 > 6 ? w / 100 : 6;
    p->min_gutter_h = h / 200 > 4 ? h / 200 : 4;
    p->min_block_w = 4;
    p->min_block_h = 4;
}

OcrLayout *ocr_layout(const OcrBinary *b, const OcrLayoutParams *params) {
    if (!b || ocr_binary_width(b) == 0 || ocr_binary_height(b) == 0) return NULL;
    OcrLayout *L = calloc(1, sizeof *L);
    if (!L) return NULL;
    OcrLayoutParams p;
    if (params) p = *params; else auto_params(b, &p);
    if (p.min_gutter_v == 0) p.min_gutter_v = 1;
    if (p.min_gutter_h == 0) p.min_gutter_h = 1;
    xy_cut(b, 0, 0, ocr_binary_width(b), ocr_binary_height(b), &p, L, 0);
    return L;
}

void ocr_layout_free(OcrLayout *L) {
    if (!L) return;
    free(L->blk);
    free(L);
}

size_t ocr_layout_count(const OcrLayout *L) { return L ? L->n : 0; }

const OcrBlock *ocr_layout_block(const OcrLayout *L, size_t i) {
    if (!L || i >= L->n) return NULL;
    return &L->blk[i];
}
