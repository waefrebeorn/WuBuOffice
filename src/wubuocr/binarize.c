/* binarize.c -- Otsu global threshold + 1-bpp ink map. */
#include "binarize.h"

#include <stdlib.h>
#include <string.h>

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
