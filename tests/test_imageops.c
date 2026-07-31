/* test_imageops.c -- unit tests for Sauvola binarization, despeckle,
 * auto-crop and empty-page helpers (clean-room, no model needed). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "image.h"
#include "binarize.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

/* build a WxH grayscale image: left half dark (ink), right half light (bg) */
static OcrImage *make_split(int W, int H) {
    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ocr_image_set(im, (size_t)x, (size_t)y, (x < W / 2) ? 20 : 230);
    return im;
}

/* build an image with a single DARK speck in an otherwise LIGHT field */
static OcrImage *make_speck(int W, int H) {
    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ocr_image_set(im, (size_t)x, (size_t)y, 230);
    ocr_image_set(im, (size_t)(W / 2), (size_t)(H / 2), 20); /* isolated ink speck */
    return im;
}

/* build a uniform (blank) image */
static OcrImage *make_uniform(int W, int H, uint8_t v) {
    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            ocr_image_set(im, (size_t)x, (size_t)y, v);
    return im;
}

int main(void) {
    /* --- Sauvola separates a bimodal (dark/light) image --- */
    {
        int W = 64, H = 32;
        OcrImage *im = make_split(W, H);
        CHECK(im != NULL, "make_split alloc");
        OcrBinary *b = ocr_binarize_sauvola(im, 15, 0.30, 128.0);
        CHECK(b != NULL, "sauvola binarize");
        if (b) {
            int left_ink = 0, right_ink = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    if (ocr_binary_ink(b, (size_t)x, (size_t)y)) {
                        if (x < W / 2) left_ink++; else right_ink++;
                    }
                }
            CHECK(left_ink > right_ink * 10, "sauvola: dark half is ink");
            CHECK(right_ink == 0, "sauvola: light half not ink");
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* --- Otsu threshold on a clean bimodal image is mid-range --- */
    {
        int W = 64, H = 32;
        OcrImage *im = make_split(W, H);
        uint8_t t = ocr_otsu_threshold(im);
        /* verify Otsu separates the two halves (left dark -> ink, right light -> bg) */
        OcrBinary *b = ocr_binarize(im, t);
        CHECK(b != NULL, "otsu binarize");
        if (b) {
            int left_ink = 0, right_ink = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    if (ocr_binary_ink(b, (size_t)x, (size_t)y)) {
                        if (x < W / 2) left_ink++; else right_ink++;
                    }
                }
            CHECK(left_ink > 0 && right_ink == 0, "otsu separates halves");
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* --- despeckle removes an isolated bright pixel --- */
    {
        int W = 40, H = 40;
        OcrImage *im = make_speck(W, H);
        OcrBinary *b = ocr_binarize(im, 128); /* dark field -> all ink */
        CHECK(b != NULL, "speck binarize");
        if (b) {
            CHECK(ocr_binary_ink(b, (size_t)(W / 2), (size_t)(H / 2)) == 1,
                  "speck present before despeckle");
            ocr_binary_despeckle(b);
            CHECK(ocr_binary_ink(b, (size_t)(W / 2), (size_t)(H / 2)) == 0,
                  "speck removed after despeckle");
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* --- auto-crop tightens to the ink bounding box --- */
    {
        int W = 100, H = 100;
        OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
        CHECK(im != NULL, "crop image alloc");
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                ocr_image_set(im, (size_t)x, (size_t)y, 230);
        /* draw a small dark square in the middle */
        for (int y = 40; y < 60; y++)
            for (int x = 40; x < 60; x++)
                ocr_image_set(im, (size_t)x, (size_t)y, 20);
        OcrBinary *b = ocr_binarize(im, 128);
        CHECK(b != NULL, "crop binarize");
        if (b) {
            OcrImage *cr = ocr_image_autocrop(im, b, 4);
            CHECK(cr != NULL, "autocrop returns image");
            if (cr) {
                size_t cw = ocr_image_width(cr), ch = ocr_image_height(cr);
                /* 20px square + 8px pad => ~28px, must be < 100 */
                CHECK(cw < (size_t)W && ch < (size_t)H, "autocrop shrinks image");
                CHECK(cw >= 20 && ch >= 20, "autocrop keeps content");
                ocr_image_free(cr);
            }
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* --- empty-page ink fraction is ~0 on a uniform image --- */
    {
        int W = 50, H = 50;
        OcrImage *im = make_uniform(W, H, 200);
        OcrBinary *b = ocr_binarize(im, 128); /* 200 > 128 -> no ink */
        CHECK(b != NULL, "uniform binarize");
        if (b) {
            double f = ocr_binary_ink_fraction(b);
            CHECK(f < 0.01, "uniform image has ~0 ink");
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    if (failures == 0) { printf("PASS: test_imageops (%d checks)\n", 0); return 0; }
    printf("FAIL: test_imageops (%d failures)\n", failures);
    return 1;
}
