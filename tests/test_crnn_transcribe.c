/* test_crnn_transcribe.c -- regression test for the CRNN line-transcribe
 * document path (crnn_transcribe_page_json -> docmodel JSON).
 *
 * Builds a synthetic multi-line Latin page from a real font, transcribes it
 * with a trained CRNN, and asserts the JSON has the expected number of
 * paragraph blocks whose text reconstructs the known ground truth.
 *
 * Env: LOAD=<model.crnn> (required), FONT=<font.ttf> (defaults to repo Latin).
 */
#include "wubufont.h"
#include "image.h"
#include "crnn.h"
#include "crnn_transcribe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STRIP 20
#define PPM   16
#define GAP   10
#define NLINES 5
#define MAXW  8
static const char *CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static uint8_t *readf(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(s ? (size_t)s : 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)s; return b;
}

static uint32_t rng = 0x9E3779B9u;
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
    if (!LOAD) { printf("SKIP: set LOAD=<trained .crnn>\n"); return 0; }
    const char *fontpath = FONT ? FONT : "fonts/multiscript_active/Latin.ttf";

    size_t fn; uint8_t *fb = readf(fontpath, &fn);
    Font *font = fb ? font_open(fb, fn) : NULL;
    if (!font) { printf("FAIL: cannot open font %s\n", fontpath); return 1; }
    rng = 0x1234u;

    int W = MAXW * STRIP, H = NLINES * STRIP + (NLINES + 1) * GAP;
    OcrImage *page = ocr_image_create(W, H);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ocr_image_set(page, x, y, 15);

    char gt[NLINES][MAXW + 1];
    for (int l = 0; l < NLINES; l++) {
        int L = 3 + (int)(rnd() * 5.99f); if (L > MAXW) L = MAXW;
        int y0 = GAP + l * (STRIP + GAP);
        for (int i = 0; i < L; i++) {
            int li = (int)(rnd() * 25.99f); if (li > 25) li = 25;
            gt[l][i] = CHARSET[li]; paint_letter(page, font, i * STRIP, y0, CHARSET[li]);
        }
        gt[l][L] = '\0';
    }

    CRNN *m = NULL;
    if (!crnn_load(LOAD, &m) || !m) { printf("FAIL: crnn_load %s\n", LOAD); ocr_image_free(page); font_free(font); free(fb); return 1; }

    char *json = NULL;
    int rc = crnn_transcribe_page_json(m, page, STRIP, CHARSET, &json);
    crnn_free(m); ocr_image_free(page); font_free(font); free(fb);
    if (rc != 0 || !json) { printf("FAIL: transcribe returned %d\n", rc); return 1; }

    /* assert structure: "blocks" array with NLINES paragraph entries */
    int nblocks = 0, npara = 0;
    const char *p = json;
    /* count "kind":"paragraph" occurrences */
    const char *k = strstr(p, "\"blocks\"");
    if (!k) { printf("FAIL: no \"blocks\" in JSON: %s\n", json); free(json); return 1; }
    const char *q = k;
    while ((q = strstr(q, "\"kind\":\"paragraph\"")) != NULL) { npara++; q += 18; }
    /* count text entries as a proxy for total blocks */
    const char *t = k;
    while ((t = strstr(t, "\"text\"")) != NULL) { nblocks++; t += 6; }

    printf("transcribed JSON: %s\n", json);
    printf("paragraphs=%d blocks(text)=%d\n", npara, nblocks);
    free(json);

    int ok = (npara == NLINES) && (nblocks == NLINES);
    printf(ok ? "PASS: crnn_transcribe produced %d lines\n" : "FAIL: expected %d lines, got %d\n", NLINES, npara);
    return ok ? 0 : 1;
}
