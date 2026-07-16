/* recognize.c -- zoning feature extraction + 1-NN template glyph classifier.
 *
 * Two template sources, same classifier:
 *   1. ocr_templates_create(): deterministic ASCII templates from the embedded
 *      font8x8 (no training data, the classic "printed text" recognizer).
 *   2. ocr_templates_create_classes() + add_sample() + finalize(): TRAIN the
 *      recognizer on a labeled dataset by averaging each class's zoning
 *      vectors over its training glyphs (the lightweight, dependency-free
 *      analog of a trained OCR head -- no neural net, no backprop).
 *
 * Recognition = 1-NN over zoning vectors (squared Euclidean distance) with a
 * distance-ratio confidence gate so ambiguous blobs are rejected (never
 * fabricated into text).
 */
#include "recognize.h"
#include "font8x8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Printable ASCII range covered by the font8x8-based recognizer. */
#define OCR_FIRST_CH 0x20   /* space */
#define OCR_LAST_CH  0x7E   /* ~ */
#define OCR_NCLASS   (OCR_LAST_CH - OCR_FIRST_CH + 1)

struct OcrTemplates {
    size_t grid;                 /* NxN zoning grid */
    size_t dim;                  /* grid*grid */
    size_t nclass;               /* number of classes (OCR_NCLASS for ASCII) */
    float *vec;                  /* nclass * dim, row-major mean zoning per class */
    float *sum;                  /* nclass * dim accumulator during training */
    size_t *cnt;                 /* samples accumulated per class */
    int    finalized;            /* 1 once finalize() has run */
    int    no_reject;            /* 1 = always return best (closed-set mode) */
    double reject_ratio;         /* best must be < reject_ratio*second */
    double reject_abs;           /* and < reject_abs*dim, else reject */
    /* Structural (topological) augmentation features, trained alongside the
     * zoning vectors to disambiguate strokes zoning alone confuses: */
    float *ar;                   /* mean aspect ratio (w/h of tight bbox) per class */
    uint8_t *holes;              /* mean hole count per class (rounded) */
    float *arsum;                /* accumulator for aspect ratio */
    float *holesum;              /* accumulator for hole count */
    double ar_w;                 /* weight of aspect-ratio mismatch in distance */
    double hole_w;               /* weight of a hole-count mismatch in distance */
    char   ch[];                 /* class -> UTF-8 char (1 byte each) */
};

/* font8x8 bit test: row byte, LSB = leftmost pixel, set bit = ink. */
static int font_ink(int codepoint, int fx, int fy) {
    if (codepoint < 0 || codepoint > 127 || fx < 0 || fx > 7 || fy < 0 || fy > 7)
        return 0;
    return (wubuocr_font8x8[codepoint][fy] >> fx) & 1;
}

/* Zone a tight ink bounding box of an 8x8 font glyph into the grid. */
static void zone_from_font(int codepoint, size_t grid, float *out) {
    int minx = 8, miny = 8, maxx = -1, maxy = -1;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (font_ink(codepoint, x, y)) {
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    if (maxx < 0) {
        for (size_t i = 0; i < grid * grid; i++) out[i] = 0.0f;
        return;
    }
    size_t bx0 = (size_t)minx, by0 = (size_t)miny;
    size_t bw = (size_t)(maxx - minx + 1), bh = (size_t)(maxy - miny + 1);
    for (size_t gy = 0; gy < grid; gy++) {
        for (size_t gx = 0; gx < grid; gx++) {
            size_t x0 = bx0 + (gx * bw) / grid, x1 = bx0 + ((gx + 1) * bw) / grid;
            size_t y0 = by0 + (gy * bh) / grid, y1 = by0 + ((gy + 1) * bh) / grid;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            size_t ink = 0, area = 0;
            for (size_t y = y0; y < y1 && y < 8; y++)
                for (size_t x = x0; x < x1 && x < 8; x++) {
                    area++;
                    if (font_ink(codepoint, (int)x, (int)y)) ink++;
                }
            out[gy * grid + gx] = area ? (float)ink / (float)area : 0.0f;
        }
    }
}

/* Count enclosed holes in a glyph plane (ink where pixel <= ink_threshold).
 * Flood background from the border, then any remaining background pixel
 * belongs to an interior hole; count connected components of those. Cheap
 * topological feature that cleanly separates O/Q/D/G and friends. */
static int count_holes(const uint8_t *px, int w, int h, uint8_t thr) {
    if (w <= 0 || h <= 0) return 0;
    size_t n = (size_t)w * h;
    uint8_t *bg = malloc(n);
    if (!bg) return 0;
    for (size_t i = 0; i < n; i++) bg[i] = (px[i] <= thr) ? 0 : 1;  /* 1 = bg (light) */
    /* flood background from the border via explicit stack */
    size_t *stk = malloc(n * sizeof *stk);
    if (!stk) { free(bg); return 0; }
    size_t sp = 0;
    /* seed border background pixels */
    for (int x = 0; x < w; x++) {
        if (bg[(size_t)x]) stk[sp++] = (size_t)x;
        if (bg[(size_t)(h-1)*w + x]) stk[sp++] = (size_t)(h-1)*w + x;
    }
    for (int y = 0; y < h; y++) {
        if (bg[(size_t)y*w]) stk[sp++] = (size_t)y*w;
        if (bg[(size_t)y*w + (w-1)]) stk[sp++] = (size_t)y*w + (w-1);
    }
    while (sp) {
        size_t p = stk[--sp];
        int cx = (int)(p % (size_t)w), cy = (int)(p / (size_t)w);
        int nb[4][2] = {{cx-1,cy},{cx+1,cy},{cx,cy-1},{cx,cy+1}};
        for (int k = 0; k < 4; k++) {
            int nx = nb[k][0], ny = nb[k][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            size_t q = (size_t)ny * w + nx;
            if (bg[q]) { bg[q] = 0; stk[sp++] = q; }
        }
    }
    /* remaining bg pixels are interior holes; count 4-connected components */
    int holes = 0;
    for (size_t i = 0; i < n; i++) {
        if (!bg[i]) continue;
        holes++;
        /* flood this hole component using the same stack buffer */
        sp = 0;
        stk[sp++] = i; bg[i] = 0;
        while (sp) {
            size_t p = stk[--sp];
            int cx = (int)(p % (size_t)w), cy = (int)(p / (size_t)w);
            int nb[4][2] = {{cx-1,cy},{cx+1,cy},{cx,cy-1},{cx,cy+1}};
            for (int k = 0; k < 4; k++) {
                int nx = nb[k][0], ny = nb[k][1];
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                size_t q = (size_t)ny * w + nx;
                if (bg[q]) { bg[q] = 0; stk[sp++] = q; }
            }
        }
    }
    free(stk);
    free(bg);
    return holes;
}

/* Zone a raw grayscale glyph plane (row-major, 0=black..255=white; ink where
 * pixel <= ink_threshold) into the grid, using the glyph's TIGHT ink bounding
 * box. Scale/translation invariant -- identical treatment to candidates. */
static void zone_from_raw(const uint8_t *px, size_t w, size_t h, size_t grid,
                          uint8_t ink_threshold, float *out) {
    size_t minx = w, miny = h, maxx = 0, maxy = 0, found = 0;
    for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
            if (px[y * w + x] <= ink_threshold) {
                found = 1;
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    if (!found) {
        for (size_t i = 0; i < grid * grid; i++) out[i] = 0.0f;
        return;
    }
    size_t bw = maxx - minx + 1, bh = maxy - miny + 1;
    for (size_t gy = 0; gy < grid; gy++) {
        for (size_t gx = 0; gx < grid; gx++) {
            size_t x0 = minx + (gx * bw) / grid, x1 = minx + ((gx + 1) * bw) / grid;
            size_t y0 = miny + (gy * bh) / grid, y1 = miny + ((gy + 1) * bh) / grid;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            size_t ink = 0, area = 0;
            for (size_t y = y0; y < y1; y++)
                for (size_t x = x0; x < x1; x++) {
                    area++;
                    if (px[y * w + x] <= ink_threshold) ink++;
                }
            out[gy * grid + gx] = area ? (float)ink / (float)area : 0.0f;
        }
    }
}

OcrTemplates *ocr_templates_create_classes(size_t grid, const char *classes,
                                            size_t nclass) {
    if (grid < 2 || grid > 16 || nclass == 0 || nclass > 4096 || !classes)
        return NULL;
    size_t dim = grid * grid;
    OcrTemplates *t = malloc(sizeof *t + nclass);
    if (!t) return NULL;
    t->grid = grid;
    t->dim = dim;
    t->nclass = nclass;
    t->finalized = 0;
    t->no_reject = 0;
    t->reject_ratio = 0.97;   /* best must be clearly closer than runner-up */
    t->reject_abs = 0.60;     /* and not absurdly far from any template */
    t->vec = malloc((size_t)nclass * dim * sizeof *t->vec);
    t->sum = calloc((size_t)nclass * dim, sizeof *t->sum);
    t->cnt = calloc(nclass, sizeof *t->cnt);
    t->ar = calloc(nclass, sizeof *t->ar);
    t->holes = calloc(nclass, sizeof *t->holes);
    t->arsum = calloc(nclass, sizeof *t->arsum);
    t->holesum = calloc(nclass, sizeof *t->holesum);
    t->ar_w = 0.0;     /* structural augmentation off by default */
    t->hole_w = 0.0;
    if (!t->vec || !t->sum || !t->cnt || !t->ar || !t->holes ||
        !t->arsum || !t->holesum) {
        free(t->vec); free(t->sum); free(t->cnt);
        free(t->ar); free(t->holes); free(t->arsum); free(t->holesum);
        free(t);
        return NULL;
    }
    for (size_t c = 0; c < nclass; c++) t->ch[c] = classes[c];
    return t;
}

OcrTemplates *ocr_templates_create(size_t grid) {
    char ascii[OCR_NCLASS];
    for (int c = 0; c < OCR_NCLASS; c++) ascii[c] = (char)(OCR_FIRST_CH + c);
    OcrTemplates *t = ocr_templates_create_classes(grid, ascii, OCR_NCLASS);
    if (!t) return NULL;
    /* Pre-fill vec from the embedded font; no accumulation needed. */
    for (int c = 0; c < OCR_NCLASS; c++)
        zone_from_font(OCR_FIRST_CH + c, grid, t->vec + (size_t)c * t->dim);
    t->finalized = 1;
    return t;
}

void ocr_templates_add_sample(OcrTemplates *t, size_t class_idx,
                              const uint8_t *px, size_t w, size_t h,
                              uint8_t ink_threshold) {
    if (!t || t->finalized || class_idx >= t->nclass || !px || w == 0 || h == 0)
        return;
    float *f = malloc(t->dim * sizeof *f);
    if (!f) return;
    zone_from_raw(px, w, h, t->grid, ink_threshold, f);
    float *s = t->sum + class_idx * t->dim;
    for (size_t i = 0; i < t->dim; i++) s[i] += f[i];
    /* structural features: tight-bbox aspect ratio + hole count */
    size_t minx = w, miny = h, maxx = 0, maxy = 0, found = 0;
    for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
            if (px[y * w + x] <= ink_threshold) {
                found = 1;
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    if (found) {
        float bw = (float)(maxx - minx + 1), bh = (float)(maxy - miny + 1);
        t->arsum[class_idx] += (bh > 0 ? bw / bh : 1.0f);
        int hc = count_holes(px, (int)w, (int)h, ink_threshold);
        t->holesum[class_idx] += (float)hc;
    }
    t->cnt[class_idx]++;
    free(f);
}

void ocr_templates_finalize(OcrTemplates *t) {
    if (!t || t->finalized) return;
    for (size_t c = 0; c < t->nclass; c++) {
        if (t->cnt[c] == 0) continue;
        float *v = t->vec + c * t->dim;
        const float *s = t->sum + c * t->dim;
        float inv = 1.0f / (float)t->cnt[c];
        for (size_t i = 0; i < t->dim; i++) v[i] = s[i] * inv;
        t->ar[c] = t->arsum[c] * inv;
        t->holes[c] = (uint8_t)(t->holesum[c] * inv + 0.5f);
    }
    t->finalized = 1;
    free(t->sum); free(t->cnt); free(t->arsum); free(t->holesum);
    t->sum = NULL; t->cnt = NULL; t->arsum = NULL; t->holesum = NULL;
}

/* Enable structural augmentation: aspect-ratio mismatch weighted by `ar_w`
 * and a hole-count mismatch by `hole_w` are added to the zoning distance.
 * (ar_w=hole_w=0 disables it; pure zoning.) Call before recognition. */
void ocr_templates_set_struct(OcrTemplates *t, double ar_w, double hole_w) {
    if (!t) return;
    t->ar_w = ar_w; t->hole_w = hole_w;
}

/* Tune / disable the confidence gate. `enabled=0` puts the recognizer in
 * closed-set mode (always return the nearest class -- correct for a benchmark
 * where every glyph is known to belong to one of the classes). Otherwise the
 * best match is rejected unless it is both within `ratio*second` and
 * `abs*dim` of the nearest template. */
void ocr_templates_set_reject(OcrTemplates *t, int enabled,
                              double ratio, double abs) {
    if (!t) return;
    t->no_reject = enabled ? 0 : 1;
    if (enabled) { t->reject_ratio = ratio; t->reject_abs = abs; }
}

int ocr_templates_struct_info(const OcrTemplates *t, size_t idx,
                              double *out_ar, int *out_holes) {
    if (!t || idx >= t->nclass) return 0;
    if (out_ar) *out_ar = (double)t->ar[idx];
    if (out_holes) *out_holes = (int)t->holes[idx];
    return 1;
}

void ocr_templates_free(OcrTemplates *t) {
    if (!t) return;
    free(t->vec);
    if (t->sum) free(t->sum);
    if (t->cnt) free(t->cnt);
    if (t->ar) free(t->ar);
    if (t->holes) free(t->holes);
    if (t->arsum) free(t->arsum);
    if (t->holesum) free(t->holesum);
    free(t);
}

char *ocr_recognize_glyph(const OcrBinary *b, const OcrBlock *glyph, void *user) {
    OcrTemplates *t = user;
    if (!t || !b || !glyph) return NULL;

    float *feat = malloc(t->dim * sizeof *feat);
    if (!feat) return NULL;

    /* Zone the candidate glyph box (reuse the raw-zoning path over the box). */
    size_t gw = glyph->x1 - glyph->x0, gh = glyph->y1 - glyph->y0;
    uint8_t *plane = NULL;
    double cand_ar = 1.0, cand_holes = 0.0;
    if (gw > 0 && gh > 0) {
        plane = malloc(gw * gh);
        if (!plane) { free(feat); return NULL; }
        for (size_t y = 0; y < gh; y++)
            for (size_t x = 0; x < gw; x++)
                plane[y * gw + x] = ocr_binary_ink(b, glyph->x0 + x, glyph->y0 + y)
                                     ? 0u : 255u;   /* ink -> 0 (dark) */
        zone_from_raw(plane, gw, gh, t->grid, 127, feat);
        /* candidate structural features (same definitions as training) */
        size_t minx = gw, miny = gh, maxx = 0, maxy = 0, found = 0;
        for (size_t y = 0; y < gh; y++)
            for (size_t x = 0; x < gw; x++)
                if (plane[y * gw + x] <= 127) {
                    found = 1;
                    if (x < minx) minx = x;
                    if (y < miny) miny = y;
                    if (x > maxx) maxx = x;
                    if (y > maxy) maxy = y;
                }
        if (found) {
            float cbw = (float)(maxx - minx + 1), cbh = (float)(maxy - miny + 1);
            cand_ar = cbh > 0 ? cbw / cbh : 1.0;
            cand_holes = (double)count_holes(plane, (int)gw, (int)gh, 127);
        }
        free(plane);
    } else {
        for (size_t i = 0; i < t->dim; i++) feat[i] = 0.0f;
    }

    /* 1-NN over classes; track best and second-best for a confidence gate. */
    double best = 1e300, second = 1e300;
    int best_c = -1;
    for (size_t c = 0; c < t->nclass; c++) {
        if (t->ch[c] == ' ') continue;   /* never emit a blank */
        const float *tv = t->vec + c * t->dim;
        double d = 0.0;
        for (size_t i = 0; i < t->dim; i++) {
            double diff = (double)feat[i] - (double)tv[i];
            d += diff * diff;
        }
        /* structural augmentation (off when ar_w==hole_w==0) */
        if (t->ar_w > 0.0) {
            double ad = (cand_ar - (double)t->ar[c]);
            d += t->ar_w * ad * ad;
        }
        if (t->hole_w > 0.0 && cand_holes != (double)t->holes[c])
            d += t->hole_w;
        if (d < best) { second = best; best = d; best_c = (int)c; }
        else if (d < second) { second = d; }
    }
    free(feat);
    if (best_c < 0) return NULL;

    /* Confidence gate: reject ambiguous or far-off blobs (never fabricate).
     * Disabled in closed-set mode (no_reject), which is correct when every
     * glyph is known to be one of the classes (e.g. a benchmark test set). */
    if (!t->no_reject) {
        if (best > t->reject_abs * (double)t->dim) return NULL;        /* too far */
        if (second < 1e299 && best > t->reject_ratio * second) return NULL; /* ambiguous */
    }

    char *out = malloc(2);
    if (!out) return NULL;
    out[0] = t->ch[best_c];
    out[1] = '\0';
    return out;
}

OcrRecognizer ocr_recognizer_fn(void) { return ocr_recognize_glyph; }
