/* binarize.c -- Otsu global threshold + Sauvola adaptive threshold +
 * despeckle + auto-crop (grayscale -> ink/background). All clean-room,
 * dependency-free; windowed stats use an integral image so they are O(W*H). */

#include "binarize.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

struct OcrBinary {
    size_t w, h;
    size_t stride;   /* bytes per row = (w+7)/8 */
    uint8_t *bits;   /* bit set == foreground ink */
};

uint8_t ocr_otsu_threshold(const OcrImage *im) {
    size_t w = ocr_image_width(im), h = ocr_image_height(im);
    if (w == 0 || h == 0) return 128;
    const uint8_t *px = ocr_image_pixels(im);
    size_t n = w * h;

    /* 256-bin histogram */
    size_t hist[256];
    memset(hist, 0, sizeof hist);
    for (size_t i = 0; i < n; i++) hist[px[i]]++;

    /* total intensity sum */
    double sum = 0.0;
    for (int t = 0; t < 256; t++) sum += (double)t * (double)hist[t];

    double sumB = 0.0;          /* weighted sum of background */
    size_t wB = 0;              /* background pixel count */
    double max_var = -1.0;
    int best = 0;

    for (int t = 0; t < 256; t++) {
        wB += hist[t];
        if (wB == 0) continue;
        size_t wF = n - wB;     /* foreground count */
        if (wF == 0) break;
        sumB += (double)t * (double)hist[t];
        double mB = sumB / (double)wB;
        double mF = (sum - sumB) / (double)wF;
        double diff = mB - mF;
        double between = (double)wB * (double)wF * diff * diff;
        if (between > max_var) { max_var = between; best = t; }
    }
    return (uint8_t)best;
}

/* Sauvola adaptive local threshold (Sauvola & Pietikäinen, 2000).
 * T(x,y) = m(x,y) * (1 + k*(s(x,y)/R - 1)), where m is the local mean and s
 * the local std-dev over a (win x win) window. Dark pixels (< T) are ink.
 * Uses an integral image + integral of squares for O(1) windows. */
uint8_t *ocr_sauvola_thresh(const OcrImage *im, int win, double k, double R) {
    size_t w = ocr_image_width(im), h = ocr_image_height(im);
    if (w == 0 || h == 0 || win < 1) return NULL;
    const uint8_t *px = ocr_image_pixels(im);
    /* integral image (uint64 to avoid overflow on 255^2 * pixels) */
    size_t *i1 = malloc((w + 1) * (h + 1) * sizeof(size_t));
    uint64_t *i2 = malloc((w + 1) * (h + 1) * sizeof(uint64_t));
    if (!i1 || !i2) { free(i1); free(i2); return NULL; }
    for (size_t y = 0; y <= h; y++) { i1[y * (w + 1)] = 0; i2[y * (w + 1)] = 0; }
    for (size_t x = 0; x <= w; x++) { i1[x] = 0; i2[x] = 0; }
    for (size_t y = 1; y <= h; y++) {
        size_t r1 = 0; uint64_t r2 = 0;
        for (size_t x = 1; x <= w; x++) {
            uint8_t v = px[(y - 1) * w + (x - 1)];
            r1 += v; r2 += (uint64_t)v * v;
            i1[y * (w + 1) + x] = i1[(y - 1) * (w + 1) + x] + r1;
            i2[y * (w + 1) + x] = i2[(y - 1) * (w + 1) + x] + r2;
        }
    }
    uint8_t *th = malloc(w * h);
    if (!th) { free(i1); free(i2); return NULL; }
    int hw = win / 2;
    for (size_t y = 0; y < h; y++) {
        int y0 = (int)y - hw; if (y0 < 0) y0 = 0;
        int y1 = (int)y + hw; if (y1 >= (int)h) y1 = (int)h - 1;
        for (size_t x = 0; x < w; x++) {
            int x0 = (int)x - hw; if (x0 < 0) x0 = 0;
            int x1 = (int)x + hw; if (x1 >= (int)w) x1 = (int)w - 1;
            size_t aw = (size_t)(x1 - x0 + 1), ah = (size_t)(y1 - y0 + 1);
            size_t area = aw * ah;
            size_t A = i1[(y1 + 1) * (w + 1) + (x1 + 1)]
                      - i1[(y0) * (w + 1) + (x1 + 1)]
                      - i1[(y1 + 1) * (w + 1) + (x0)]
                      + i1[(y0) * (w + 1) + (x0)];
            uint64_t B = i2[(y1 + 1) * (w + 1) + (x1 + 1)]
                       - i2[(y0) * (w + 1) + (x1 + 1)]
                       - i2[(y1 + 1) * (w + 1) + (x0)]
                       + i2[(y0) * (w + 1) + (x0)];
            double m = (double)A / (double)area;
            double var = (double)B / (double)area - m * m;
            if (var < 0) var = 0;
            double s = sqrt(var);
            double T = m * (1.0 + k * (s / R - 1.0));
            if (T < 0) T = 0; if (T > 255) T = 255;
            th[y * w + x] = (uint8_t)T;
        }
    }
    free(i1); free(i2);
    return th;
}

OcrBinary *ocr_binarize(const OcrImage *im, uint8_t threshold) {
    size_t w = ocr_image_width(im), h = ocr_image_height(im);
    if (w == 0 || h == 0) return NULL;
    OcrBinary *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->w = w; b->h = h;
    b->stride = (w + 7) / 8;
    b->bits = calloc(b->stride * h, 1);
    if (!b->bits) { free(b); return NULL; }

    const uint8_t *px = ocr_image_pixels(im);
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            if (px[y * w + x] <= threshold) {   /* dark == ink */
                b->bits[y * b->stride + (x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
            }
        }
    }
    return b;
}

/* Binarize with a per-pixel Sauvola threshold map (adaptive). */
OcrBinary *ocr_binarize_sauvola(const OcrImage *im, int win, double k, double R) {
    size_t w = ocr_image_width(im), h = ocr_image_height(im);
    if (w == 0 || h == 0) return NULL;
    uint8_t *th = ocr_sauvola_thresh(im, win, k, R);
    if (!th) return NULL;
    const uint8_t *px = ocr_image_pixels(im);
    OcrBinary *b = malloc(sizeof *b);
    if (!b) { free(th); return NULL; }
    b->w = w; b->h = h; b->stride = (w + 7) / 8;
    b->bits = calloc(b->stride * h, 1);
    if (!b->bits) { free(th); free(b); return NULL; }
    for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
            if (px[y * w + x] <= th[y * w + x])
                b->bits[y * b->stride + (x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
    free(th);
    return b;
}

/* Remove isolated foreground specks: a foreground bit with no 8-connected
 * foreground neighbour is noise and is cleared. Two passes (open by 1). */
void ocr_binary_despeckle(OcrBinary *b) {
    if (!b) return;
    size_t w = b->w, h = b->h;
    uint8_t *nb = malloc(b->stride * h);
    if (!nb) return;
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            int ink = ocr_binary_ink(b, x, y);
            if (!ink) { nb[y * b->stride + (x >> 3)] &= (uint8_t)~(0x80u >> (x & 7)); continue; }
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = (int)x + dx, ny = (int)y + dy;
                    if (nx < 0 || ny < 0 || (size_t)nx >= w || (size_t)ny >= h) continue;
                    n += ocr_binary_ink(b, (size_t)nx, (size_t)ny);
                }
            if (n == 0)
                nb[y * b->stride + (x >> 3)] &= (uint8_t)~(0x80u >> (x & 7));
            else
                nb[y * b->stride + (x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
        }
    }
    memcpy(b->bits, nb, b->stride * h);
    free(nb);
}

/* Fraction of pixels that are ink (0..1). Used for empty-page / low-ink
 * detection so the pipeline can bail out early on blank scans. */
double ocr_binary_ink_fraction(const OcrBinary *b) {
    if (!b || b->w == 0 || b->h == 0) return 0.0;
    size_t total = b->w * b->h, ink = 0;
    for (size_t i = 0; i < b->stride * b->h; i++) {
        uint8_t v = b->bits[i];
        while (v) { ink += v & 1; v >>= 1; }
    }
    return (double)ink / (double)total;
}

/* Crop to the ink bounding box (auto-crop margins). Returns a new OcrImage, or
 * NULL if the source is empty / allocation fails. Caller frees. */
OcrImage *ocr_image_autocrop(const OcrImage *im, const OcrBinary *b, int pad) {
    if (!im || !b) return NULL;
    size_t w = b->w, h = b->h;
    long x0 = (long)w, y0 = (long)h, x1 = -1, y1 = -1;
    for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++)
            if (ocr_binary_ink(b, x, y)) {
                if ((long)x < x0) x0 = (long)x;
                if ((long)x > x1) x1 = (long)x;
                if ((long)y < y0) y0 = (long)y;
                if ((long)y > y1) y1 = (long)y;
            }
    if (x1 < x0 || y1 < y0) return NULL;  /* empty */
    x0 -= pad; if (x0 < 0) x0 = 0;
    y0 -= pad; if (y0 < 0) y0 = 0;
    x1 += pad; if (x1 >= (long)w) x1 = (long)w - 1;
    y1 += pad; if (y1 >= (long)h) y1 = (long)h - 1;
    size_t cw = (size_t)(x1 - x0 + 1), ch = (size_t)(y1 - y0 + 1);
    OcrImage *out = ocr_image_create(cw, ch);
    if (!out) return NULL;
    const uint8_t *px = ocr_image_pixels(im);
    for (size_t y = 0; y < ch; y++)
        for (size_t x = 0; x < cw; x++)
            ocr_image_set(out, x, y, px[((size_t)y0 + y) * w + ((size_t)x0 + x)]);
    return out;
}

void ocr_binary_free(OcrBinary *b) {
    if (!b) return;
    free(b->bits);
    free(b);
}

size_t ocr_binary_width(const OcrBinary *b)  { return b ? b->w : 0; }
size_t ocr_binary_height(const OcrBinary *b) { return b ? b->h : 0; }

int ocr_binary_ink(const OcrBinary *b, size_t x, size_t y) {
    if (!b || x >= b->w || y >= b->h) return 0;
    return (b->bits[y * b->stride + (x >> 3)] >> (7 - (x & 7))) & 1;
}

OcrBinary *ocr_binary_from_raw(const uint8_t *px, size_t w, size_t h,
                               uint8_t ink_threshold) {
    if (!px || w == 0 || h == 0) return NULL;
    OcrBinary *b = malloc(sizeof *b);
    if (!b) return NULL;
    b->w = w; b->h = h;
    b->stride = (w + 7) / 8;
    b->bits = calloc(b->stride * h, 1);
    if (!b->bits) { free(b); return NULL; }
    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            if (px[y * w + x] <= ink_threshold) {
                b->bits[y * b->stride + (x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
            }
        }
    }
    return b;
}
