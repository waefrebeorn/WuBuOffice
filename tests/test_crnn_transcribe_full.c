/* test_crnn_transcribe_full.c -- regression test for the CRNN line-transcribe
 * path with the full document alphabet (a-z A-Z 0-9 space + punctuation).
 * Mirrors test_crnn_transcribe.c but exercises the multi-class (69-class)
 * model so the CTC >64-class code path (prob/acc buffers) stays covered.
 *
 * Env: LOAD=<full-model.crnn> (required), FONT=<font.ttf>, CHARS=<alphabet>.
 */
#include "wubufont.h"
#include "image.h"
#include "crnn.h"
#include "crnn_transcribe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRIP 20
#define PPM   16
#define GAP   10
#define NLINES 5
#define MAXW  12
static const char *DOC_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'-";

static uint8_t *readf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(s ? (size_t)s : 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)s; return b;
}
static uint32_t rng = 0x1234u;
static float rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                        return (float)(rng & 0xFFFFFF) / (float)0xFFFFFF; }
static void paint_letter(OcrImage *im, Font *f, int x0, int y0, char ch) {
    uint8_t *bits = NULL; int w = 0, h = 0;
    if (!font_rasterize(f, (uint32_t)ch, PPM, &bits, &w, &h)) { if (bits) free(bits); return; }
    int ox = (STRIP - w) / 2; if (ox < 0) ox = 0;
    int oy = (STRIP - h) / 2; if (oy < 0) oy = 0;
    for (int y = 0; y < h && y + oy < STRIP; y++)
        for (int x = 0; x < w && x + ox < STRIP; x++)
            if (bits[y * w + x]) ocr_image_set(im, x0 + ox + x, y0 + oy + y, 235);
    free(bits);
}

int main(void) {
    const char *LOAD = getenv("LOAD");
    const char *FONT = getenv("FONT");
    const char *CH = getenv("CHARS") ? getenv("CHARS") : DOC_CHARS;
    if (!LOAD) { printf("SKIP: set LOAD=<trained .crnn>\n"); return 0; }
    const char *fontpath = FONT ? FONT : "fonts/multiscript_active/Latin.ttf";

    size_t fn; uint8_t *fb = readf(fontpath, &fn);
    Font *font = fb ? font_open(fb, fn) : NULL;
    if (!font) { printf("FAIL: cannot open font %s\n", fontpath); return 1; }
    rng = 0xCAFEu;

    int W = MAXW * STRIP, H = NLINES * STRIP + (NLINES + 1) * GAP;
    OcrImage *page = ocr_image_create(W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ocr_image_set(page, x, y, 15);

    int nch = (int)strlen(CH);
    for (int l = 0; l < NLINES; l++) {
        int L = 3 + (int)(rnd() * (MAXW - 3)); if (L > MAXW) L = MAXW;
        int y0 = GAP + l * (STRIP + GAP);
        for (int i = 0; i < L; i++) {
            int li = (int)(rnd() * nch); if (li > nch - 1) li = nch - 1;
            char c = CH[li];
            if (c != ' ') paint_letter(page, font, i * STRIP, y0, c);
        }
    }

    CRNN *m = NULL;
    if (!crnn_load(LOAD, &m) || !m) { printf("FAIL: crnn_load %s\n", LOAD); ocr_image_free(page); font_free(font); free(fb); return 1; }

    char *json = NULL;
    int rc = crnn_transcribe_page_json(m, page, STRIP, CH, NULL, &json);
    crnn_free(m); ocr_image_free(page); font_free(font); free(fb);
    if (rc != 0 || !json) { printf("FAIL: transcribe returned %d\n", rc); return 1; }

    int npara = 0;
    const char *q = strstr(json, "\"blocks\"");
    if (!q) { printf("FAIL: no \"blocks\" in JSON: %s\n", json); free(json); return 1; }
    while ((q = strstr(q, "\"kind\":\"paragraph\"")) != NULL) { npara++; q += 18; }

    printf("transcribed JSON: %s\n", json);
    printf("paragraphs=%d\n", npara);
    free(json);
    int ok = (npara == NLINES);
    printf(ok ? "PASS: crnn_transcribe (full charset) produced %d lines\n" : "FAIL: expected %d lines, got %d\n", NLINES, npara);
    return ok ? 0 : 1;
}
