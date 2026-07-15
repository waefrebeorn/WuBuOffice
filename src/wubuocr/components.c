/* components.c -- 8-connected component labeling via iterative flood fill. */
#include "components.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct { OcrBlock box; size_t area; } Comp;

struct OcrComponents {
    Comp *c;
    size_t n, cap;
};

typedef struct { size_t x, y; } Pt;

static int comp_push(OcrComponents *cc, Comp v) {
    if (cc->n == cc->cap) {
        size_t nc = cc->cap ? cc->cap * 2 : 32;
        Comp *nb = realloc(cc->c, nc * sizeof *nb);
        if (!nb) return -1;
        cc->c = nb; cc->cap = nc;
    }
    cc->c[cc->n++] = v;
    return 0;
}

/* Sort into reading order: group by row bands (top-to-bottom), left-to-right
 * within a band. Band tolerance = half the median-ish height (use each box's
 * own height as the comparison slack, which is robust for text lines). */
static int comp_cmp(const void *pa, const void *pb) {
    const Comp *a = pa, *b = pb;
    size_t ay = a->box.y0, by = b->box.y0;
    size_t ah = a->box.y1 - a->box.y0;
    size_t slack = ah / 2 + 1;
    if (ay + slack < by) return -1;
    if (by + slack < ay) return 1;
    /* same line band -> left to right */
    if (a->box.x0 < b->box.x0) return -1;
    if (a->box.x0 > b->box.x0) return 1;
    return 0;
}

OcrComponents *ocr_components(const OcrBinary *b, size_t x0, size_t y0,
                             size_t x1, size_t y1, size_t min_area) {
    if (!b) return NULL;
    size_t W = ocr_binary_width(b), H = ocr_binary_height(b);
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;
    if (x1 <= x0 || y1 <= y0) {
        OcrComponents *empty = calloc(1, sizeof *empty);
        return empty;
    }
    size_t rw = x1 - x0, rh = y1 - y0;
    size_t rn = rw * rh;

    OcrComponents *cc = calloc(1, sizeof *cc);
    if (!cc) return NULL;

    uint8_t *seen = calloc(rn, 1);           /* visited flags, region-local */
    Pt *stack = malloc(rn * sizeof *stack);  /* worst-case whole region */
    if (!seen || !stack) { free(seen); free(stack); free(cc); return NULL; }

    for (size_t sy = y0; sy < y1; sy++) {
        for (size_t sx = x0; sx < x1; sx++) {
            size_t li = (sy - y0) * rw + (sx - x0);
            if (seen[li] || !ocr_binary_ink(b, sx, sy)) continue;

            /* flood fill this component */
            size_t sp = 0;
            stack[sp++] = (Pt){ sx, sy };
            seen[li] = 1;
            size_t minx = sx, miny = sy, maxx = sx, maxy = sy, area = 0;

            while (sp) {
                Pt p = stack[--sp];
                area++;
                if (p.x < minx) minx = p.x;
                if (p.y < miny) miny = p.y;
                if (p.x > maxx) maxx = p.x;
                if (p.y > maxy) maxy = p.y;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        long nx = (long)p.x + dx, ny = (long)p.y + dy;
                        if (nx < (long)x0 || nx >= (long)x1 ||
                            ny < (long)y0 || ny >= (long)y1) continue;
                        size_t ni = ((size_t)ny - y0) * rw + ((size_t)nx - x0);
                        if (seen[ni]) continue;
                        if (!ocr_binary_ink(b, (size_t)nx, (size_t)ny)) continue;
                        seen[ni] = 1;
                        stack[sp++] = (Pt){ (size_t)nx, (size_t)ny };
                    }
                }
            }

            if (area >= min_area) {
                Comp comp = { { minx, miny, maxx + 1, maxy + 1 }, area };
                if (comp_push(cc, comp) != 0) {
                    free(seen); free(stack);
                    ocr_components_free(cc);
                    return NULL;
                }
            }
        }
    }

    free(seen);
    free(stack);
    if (cc->n > 1) qsort(cc->c, cc->n, sizeof *cc->c, comp_cmp);
    return cc;
}

OcrComponents *ocr_components_in_block(const OcrBinary *b, const OcrBlock *blk,
                                      size_t min_area) {
    if (!blk) return NULL;
    return ocr_components(b, blk->x0, blk->y0, blk->x1, blk->y1, min_area);
}

void ocr_components_free(OcrComponents *c) {
    if (!c) return;
    free(c->c);
    free(c);
}

size_t ocr_components_count(const OcrComponents *c) { return c ? c->n : 0; }

const OcrBlock *ocr_components_box(const OcrComponents *c, size_t i) {
    if (!c || i >= c->n) return NULL;
    return &c->c[i].box;
}

size_t ocr_components_area(const OcrComponents *c, size_t i) {
    if (!c || i >= c->n) return 0;
    return c->c[i].area;
}
