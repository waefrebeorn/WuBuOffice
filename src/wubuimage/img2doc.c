/* img2doc.c -- image -> OCR'd document bridge (see img2doc.h). */
#define _POSIX_C_SOURCE 200809L
#include "img2doc.h"
#include "png.h"
#include "wubuocr.h"     /* OcrImage, ocr_page_analyze, ocr_page_to_docmodel_json */
#include "fontbank.h"    /* OcrFontBank, ocr_fontbank_* */
#include "wubufont.h"    /* Font, font_open */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Build an OcrFontBank from whatever real system fonts are present (the
 * "study many font types" idea: more fonts => more robust voting). The caller
 * keeps the returned bank + the underlying Font* objects alive for the life of
 * the recognizer. Returns NULL if no usable font was found. */
OcrFontBank *img2doc_default_bank(void) {
    static const char *candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/opentype/liberation/LiberationSans-Regular.ttf",
        NULL
    };
    Font *fonts[OCR_FONTBANK_MAX];
    size_t nf = 0, kept = 0;
    for (size_t i = 0; candidates[i] && nf < OCR_FONTBANK_MAX; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
        if (n <= 0) { fclose(f); continue; }
        uint8_t *buf = malloc((size_t)n);
        if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); continue; }
        fclose(f);
        Font *fo = font_open_owned(buf, (size_t)n, 1);  /* self-contained copy */
        free(buf);                                      /* blob now owned by Font */
        if (!fo) continue;
        fonts[nf++] = fo;
        kept++;
    }
    if (kept == 0) return NULL;
    OcrFontBank *bank = ocr_fontbank_build((const void *const *)fonts, nf, 5, 48);
    /* The bank copies every template into its own heap during build, so the
     * Font* objects + their now-owned blobs can be released here -- no leak,
     * and the bank stays fully usable after this returns. */
    for (size_t i = 0; i < nf; i++) font_free(fonts[i]);
    return bank;
}

/* Recognize text from a decoded PNG blob using the supplied bank. */
char *img2doc_recognize_png(const uint8_t *png, size_t len, OcrFontBank *bank) {
    if (!bank) return NULL;
    PngImage *im = png_decode(png, len);
    if (!im) return NULL;
    char *doc = img2doc_recognize_rgba(png_rgba(im), png_width(im), png_height(im),
                                       ocr_fontbank_recognizer(), bank);
    png_free(im);
    return doc;
}

/* Build an OcrImage (8-bit grayscale, Rec.601 luma) from an RGBA plane and
 * run the recognizer. Returns malloc'd doc-model JSON or NULL. */
char *img2doc_recognize_rgba(const uint8_t *rgba, size_t w, size_t h,
                             OcrRecognizer rec, void *user) {
    if (!rgba || w == 0 || h == 0 || !rec) return NULL;

    OcrImage *im = ocr_image_create(w, h);
    if (!im) return NULL;

    for (size_t y = 0; y < h; y++) {
        for (size_t x = 0; x < w; x++) {
            const uint8_t *px = rgba + (y * w + x) * 4;
            int lum = (px[0] * 299 + px[1] * 587 + px[2] * 114) / 1000;
            ocr_image_set(im, x, y, (uint8_t)lum);
        }
    }

    OcrPage *pg = ocr_page_analyze(im, NULL, rec, user);
    ocr_image_free(im);
    if (!pg) return NULL;

    char *doc = ocr_page_to_docmodel_json(pg);
    ocr_page_free(pg);
    return doc;
}
