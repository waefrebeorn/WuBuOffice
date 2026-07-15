/* recognize.c -- zoning feature extraction + 1-NN template glyph classifier. */
#include "recognize.h"
#include "font8x8.h"

#include <stdlib.h>
#include <string.h>

/* Printable ASCII range covered by the classifier. */
#define OCR_FIRST_CH 0x20   /* space */
#define OCR_LAST_CH  0x7E   /* ~ */
#define OCR_NCLASS   (OCR_LAST_CH - OCR_FIRST_CH + 1)

struct OcrTemplates {
    size_t grid;                 /* NxN zoning grid */
    size_t dim;                  /* grid*grid */
    float *vec;                  /* OCR_NCLASS * dim, row-major per class */
    char   ch[OCR_NCLASS];       /* class -> ASCII char */
};

/* font8x8 bit test: row byte, LSB = leftmost pixel, set bit = ink. */
static int font_ink(int codepoint, int fx, int fy) {
    if (codepoint < 0 || codepoint > 127 || fx < 0 || fx > 7 || fy < 0 || fy > 7)
        return 0;
    return (wubuocr_font8x8[codepoint][fy] >> fx) & 1;
}

/* Zoning feature vector for the embedded font glyph `codepoint`. To match the
 * candidate normalization (which zones a glyph's TIGHT ink bounding box), we
 * first find the glyph's ink bbox within the 8x8 cell and zone over that. This
 * makes template and candidate scale-invariant in the same way. */
static void zone_from_font(int codepoint, size_t grid, float *out) {
    /* tight ink bbox in the 8x8 cell */
    int minx = 8, miny = 8, maxx = -1, maxy = -1;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (font_ink(codepoint, x, y)) {
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    if (maxx < 0) { /* empty glyph (e.g. space) -> all-zero vector */
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

/* Zoning feature vector for a candidate glyph box within the ink map. Scales
 * the box's own bounding rectangle into the grid (scale-invariant). */
static void zone_from_glyph(const OcrBinary *b, const OcrBlock *g,
                            size_t grid, float *out) {
    size_t gw = g->x1 - g->x0, gh = g->y1 - g->y0;
    if (gw == 0) gw = 1;
    if (gh == 0) gh = 1;
    for (size_t gy = 0; gy < grid; gy++) {
        for (size_t gx = 0; gx < grid; gx++) {
            size_t x0 = g->x0 + (gx * gw) / grid;
            size_t x1 = g->x0 + ((gx + 1) * gw) / grid;
            size_t y0 = g->y0 + (gy * gh) / grid;
            size_t y1 = g->y0 + ((gy + 1) * gh) / grid;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            size_t ink = 0, area = 0;
            for (size_t y = y0; y < y1; y++)
                for (size_t x = x0; x < x1; x++) {
                    area++;
                    if (ocr_binary_ink(b, x, y)) ink++;
                }
            out[gy * grid + gx] = area ? (float)ink / (float)area : 0.0f;
        }
    }
}

OcrTemplates *ocr_templates_create(size_t grid) {
    if (grid < 2 || grid > 16) return NULL;
    OcrTemplates *t = malloc(sizeof *t);
    if (!t) return NULL;
    t->grid = grid;
    t->dim = grid * grid;
    t->vec = malloc((size_t)OCR_NCLASS * t->dim * sizeof *t->vec);
    if (!t->vec) { free(t); return NULL; }
    for (int c = 0; c < OCR_NCLASS; c++) {
        int cp = OCR_FIRST_CH + c;
        t->ch[c] = (char)cp;
        zone_from_font(cp, grid, t->vec + (size_t)c * t->dim);
    }
    return t;
}

void ocr_templates_free(OcrTemplates *t) {
    if (!t) return;
    free(t->vec);
    free(t);
}

char *ocr_recognize_glyph(const OcrBinary *b, const OcrBlock *glyph, void *user) {
    OcrTemplates *t = user;
    if (!t || !b || !glyph) return NULL;

    float *feat = malloc(t->dim * sizeof *feat);
    if (!feat) return NULL;
    zone_from_glyph(b, glyph, t->grid, feat);

    /* 1-NN over classes; track best and second-best for a confidence gate.
     * The space glyph (all-zero template) is skipped: an empty box would match
     * it trivially, but the layout stage already excludes blank regions. */
    double best = 1e300, second = 1e300;
    int best_c = -1;
    for (int c = 0; c < OCR_NCLASS; c++) {
        if (t->ch[c] == ' ') continue;
        const float *tv = t->vec + (size_t)c * t->dim;
        double d = 0.0;
        for (size_t i = 0; i < t->dim; i++) {
            double diff = (double)feat[i] - (double)tv[i];
            d += diff * diff;
        }
        if (d < best) { second = best; best = d; best_c = c; }
        else if (d < second) { second = d; }
    }
    free(feat);
    if (best_c < 0) return NULL;

    /* Confidence gate: reject when the best match is not meaningfully closer
     * than the runner-up (ambiguous), so noise is not turned into text.
     * best must be < 0.85 * second (strictly better) AND absolutely small. */
    if (best > 0.60 * (double)t->dim) return NULL;          /* too far from any glyph */
    if (second < 1e299 && best > 0.97 * second) return NULL; /* genuinely ambiguous */

    char *out = malloc(2);
    if (!out) return NULL;
    out[0] = t->ch[best_c];
    out[1] = '\0';
    return out;
}

OcrRecognizer ocr_recognizer_fn(void) { return ocr_recognize_glyph; }
