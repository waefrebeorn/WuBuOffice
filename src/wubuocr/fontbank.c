/* fontbank.c -- multi-font zoning template bank + per-font voting.
 *
 * See fontbank.h. Builds zoning templates from REAL fonts (rasterized with
 * the clean-room wubufont rasterizer) and classifies a candidate glyph by
 * majority vote across every font in the bank. Reuses wubufont's
 * rasterizer as the single source of glyph truth -- no duplicate decoder.
 */
#include "fontbank.h"
#include "wubufont.h"   /* Font, font_open, font_rasterize, font_cmap, font_free */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Printable ASCII range covered (matches recognize.c / font8x8). */
#define FB_FIRST_CH 0x20   /* space */
#define FB_LAST_CH  0x7E   /* ~ */
#define FB_NCLASS   (FB_LAST_CH - FB_FIRST_CH + 1)

struct OcrFontBank {
    size_t grid;                 /* NxN zoning grid */
    size_t dim;                  /* grid*grid */
    size_t nfonts;               /* number of contributing fonts */
    /* per-font templates: fonts[n] -> dim floats per class, row-major
     * (FB_NCLASS * dim). A NULL row means that font had no usable
     * template for the class (e.g. glyph missing from the font). */
    float *vec[OCR_FONTBANK_MAX];
    int    have[OCR_FONTBANK_MAX][FB_NCLASS];  /* 1 if class template present */
};

/* Inline ink test over a 1-bit wubufont bitmap (0/1, row-major w*h). */
static int fb_ink(const uint8_t *bits, int w, int h, int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return 0;
    return bits[(size_t)y * w + x] ? 1 : 0;
}

/* Zoning feature vector for a rendered glyph bitmap (wubufont output).
 * Zones the glyph's TIGHT ink bounding box so the template is
 * scale-invariant in exactly the same way the candidate is zoned below.
 *
 * Zone boundaries use FLOATING-POINT math over the box extent, and each
 * output zone sums the ink of every source pixel whose center falls inside
 * it (with a partial-pixel fringe via the pixel's fractional coverage of the
 * zone). This avoids the integer-division collapse that saturated every
 * column zone to 1.0 for thin glyphs (e.g. a 4px-wide 'l'), which previously
 * made all narrow glyphs indistinguishable. */
static void fb_zone_bitmap(const uint8_t *bits, int w, int h,
                           size_t grid, float *out) {
    int minx = w, miny = h, maxx = -1, maxy = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (fb_ink(bits, w, h, x, y)) {
                if (x < minx) minx = x;
                if (y < miny) miny = y;
                if (x > maxx) maxx = x;
                if (y > maxy) maxy = y;
            }
    if (maxx < 0) {  /* empty glyph (e.g. space) -> all-zero vector */
        for (size_t i = 0; i < grid * grid; i++) out[i] = 0.0f;
        return;
    }
    double bx0 = (double)minx, by0 = (double)miny;
    double bw = (double)(maxx - minx + 1), bh = (double)(maxy - miny + 1);
    for (size_t gy = 0; gy < grid; gy++)
        for (size_t gx = 0; gx < grid; gx++) {
            double zx0 = bx0 + bw * ((double)gx) / (double)grid;
            double zx1 = bx0 + bw * ((double)(gx + 1)) / (double)grid;
            double zy0 = by0 + bh * ((double)gy) / (double)grid;
            double zy1 = by0 + bh * ((double)(gy + 1)) / (double)grid;
            double ink = 0.0, area = 0.0;
            int ix0 = (int)floor(zx0), ix1 = (int)ceil(zx1);
            int iy0 = (int)floor(zy0), iy1 = (int)ceil(zy1);
            for (int y = iy0; y < iy1; y++)
                for (int x = ix0; x < ix1; x++) {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    /* fractional coverage of this pixel by the zone rect */
                    double ox0 = (x > zx0) ? (double)x : zx0;
                    double ox1 = (x + 1 < zx1) ? (double)(x + 1) : zx1;
                    double oy0 = (y > zy0) ? (double)y : zy0;
                    double oy1 = (y + 1 < zy1) ? (double)(y + 1) : zy1;
                    double cov = (ox1 - ox0) * (oy1 - oy0);
                    if (cov <= 0.0) continue;
                    area += cov;
                    if (fb_ink(bits, w, h, x, y)) ink += cov;
                }
            out[gy * grid + gx] = area > 0.0f ? (float)(ink / area) : 0.0f;
        }
}

/* Zoning feature vector for a candidate glyph box within the ink map.
 * Mirrors fb_zone_bitmap using float zone boundaries over the box rect. */
static void fb_zone_glyph(const OcrBinary *b, const OcrBlock *g,
                          size_t grid, float *out) {
    double gx0 = (double)g->x0, gy0 = (double)g->y0;
    double gw = (double)(g->x1 - g->x0), gh = (double)(g->y1 - g->y0);
    if (gw <= 0) gw = 1.0;
    if (gh <= 0) gh = 1.0;
    for (size_t gy = 0; gy < grid; gy++)
        for (size_t gx = 0; gx < grid; gx++) {
            double zx0 = gx0 + gw * ((double)gx) / (double)grid;
            double zx1 = gx0 + gw * ((double)(gx + 1)) / (double)grid;
            double zy0 = gy0 + gh * ((double)gy) / (double)grid;
            double zy1 = gy0 + gh * ((double)(gy + 1)) / (double)grid;
            double ink = 0.0, area = 0.0;
            int ix0 = (int)floor(zx0), ix1 = (int)ceil(zx1);
            int iy0 = (int)floor(zy0), iy1 = (int)ceil(zy1);
            for (int y = iy0; y < iy1; y++)
                for (int x = ix0; x < ix1; x++) {
                    double ox0 = (x > zx0) ? (double)x : zx0;
                    double ox1 = (x + 1 < zx1) ? (double)(x + 1) : zx1;
                    double oy0 = (y > zy0) ? (double)y : zy0;
                    double oy1 = (y + 1 < zy1) ? (double)(y + 1) : zy1;
                    double cov = (ox1 - ox0) * (oy1 - oy0);
                    if (cov <= 0.0) continue;
                    area += cov;
                    if (ocr_binary_ink(b, (size_t)x, (size_t)y)) ink += cov;
                }
            out[gy * grid + gx] = area > 0.0f ? (float)(ink / area) : 0.0f;
        }
}

OcrFontBank *ocr_fontbank_build(const void *const *fonts, size_t nfonts,
                                size_t grid, int ppm) {
    if (grid < 2 || grid > 16 || nfonts == 0) return NULL;
    if (nfonts > OCR_FONTBANK_MAX) nfonts = OCR_FONTBANK_MAX;
    if (ppm <= 0) ppm = 32;

    OcrFontBank *bank = malloc(sizeof *bank);
    if (!bank) return NULL;
    memset(bank, 0, sizeof *bank);
    bank->grid = grid;
    bank->dim = grid * grid;

    size_t ncontrib = 0;
    for (size_t f = 0; f < nfonts; f++) {
        const Font *fo = (const Font *)fonts[f];
        if (!fo) continue;
        float *vec = malloc((size_t)FB_NCLASS * bank->dim * sizeof *vec);
        if (!vec) continue;
        int any = 0;
        for (int c = 0; c < FB_NCLASS; c++) {
            bank->have[f][c] = 0;
            int cp = FB_FIRST_CH + c;
            /* skip the space template (handled by abstaining in classify) */
            if (c == 0) { vec[(size_t)c * bank->dim] = 0.0f; continue; }
            int w = 0, h = 0; uint8_t *bits = NULL;
            if (!font_rasterize(fo, (uint32_t)cp, ppm, &bits, &w, &h) || !bits) {
                vec[(size_t)c * bank->dim] = 0.0f;
                continue;
            }
            fb_zone_bitmap(bits, w, h, grid, vec + (size_t)c * bank->dim);
            free(bits);
            bank->have[f][c] = 1;
            any = 1;
        }
        if (!any) { free(vec); continue; }
        bank->vec[f] = vec;
        bank->nfonts++;
        ncontrib++;
    }

    if (ncontrib == 0) { free(bank); return NULL; }
    return bank;
}

void ocr_fontbank_free(OcrFontBank *bank) {
    if (!bank) return;
    for (size_t f = 0; f < OCR_FONTBANK_MAX; f++) free(bank->vec[f]);
    free(bank);
}

size_t ocr_fontbank_font_count(const OcrFontBank *bank) {
    return bank ? bank->nfonts : 0;
}

char *ocr_fontbank_recognize(const OcrBinary *b, const OcrBlock *glyph,
                             void *user) {
    OcrFontBank *bank = (OcrFontBank *)user;
    if (!bank || !b || !glyph || bank->nfonts == 0) return NULL;

    float *feat = malloc(bank->dim * sizeof *feat);
    if (!feat) return NULL;
    fb_zone_glyph(b, glyph, bank->grid, feat);

    /* Each font casts a CONFIDENCE-WEIGHTED vote: it contributes
     * -best_distance (closer match => higher score) to the class it
     * matches best, gated by the same confidence rule as the single-font
     * recognizer. Summing scores (instead of 1-vote-per-font) means
     * a glyph that one font recognizes confidently wins even if another
     * font's template is ambiguous -- no spurious tie-rejection that
     * would otherwise blank a clearly-readable glyph. */
    double score[FB_NCLASS];
    memset(score, 0, sizeof score);

    for (size_t f = 0; f < OCR_FONTBANK_MAX; f++) {
        if (!bank->vec[f]) continue;
        double best = 1e300, second = 1e300;
        int best_c = -1;
        for (int c = 0; c < FB_NCLASS; c++) {
            if (c == 0) continue;              /* skip space */
            if (!bank->have[f][c]) continue;     /* font lacks this glyph */
            const float *tv = bank->vec[f] + (size_t)c * bank->dim;
            double d = 0.0;
            for (size_t i = 0; i < bank->dim; i++) {
                double diff = (double)feat[i] - (double)tv[i];
                d += diff * diff;
            }
            if (d < best) { second = best; best = d; best_c = c; }
            else if (d < second) { second = d; }
        }
        if (best_c < 0) continue;
        /* confidence gate: reject wildly-off or genuinely ambiguous matches
         * so noise is not turned into text. */
        if (best > 0.60 * (double)bank->dim) continue;
        if (second < 1e299 && best > 0.97 * second) continue;
        /* weight by closeness: nearer template => larger positive contribution */
        score[best_c] += (double)bank->dim - best;
    }

    free(feat);

    /* highest cumulative confidence across the bank wins (no tie issue:
     * floating sums are extremely unlikely to be exactly equal). */
    int best_c = -1;
    double best_s = 0.0;
    for (int c = 1; c < FB_NCLASS; c++) {
        if (score[c] > best_s) { best_s = score[c]; best_c = c; }
    }
    if (best_c < 0 || best_s <= 0.0) return NULL;

    char *out = malloc(2);
    if (!out) return NULL;
    out[0] = (char)(FB_FIRST_CH + best_c);
    out[1] = '\0';
    return out;
}

OcrRecognizer ocr_fontbank_recognizer(void) { return ocr_fontbank_recognize; }
