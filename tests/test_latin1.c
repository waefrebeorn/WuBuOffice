/* test_latin1.c -- Latin-1 tier (English-first, Latin second) for the
 * multi-font bank.
 *
 * Verifies the user's "English first and Latin language second" requirement:
 *   1. The English+Latin composite class set has 191 classes (95 + 96) and
 *      Latin glyph strings are valid 2-byte UTF-8 (e.g. e-acute = 0xC3 0xA9).
 *   2. A bank built over the composite set actually READS accented Latin
 *      glyphs when they are scattered + warped on a page (recall > 0 through
 *      the real OCR pipeline). Skips (exit 0) if no system font exists.
 */
#include "fontbank.h"
#include "wubufont.h"
#include "gauntlet.h"
#include "page_compose.h"
#include "latin1.h"
#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static const char *candidate_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/mnt/c/Windows/Fonts/arial.ttf",
    NULL
};

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f); rewind(f);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    *n = rd;
    return b;
}

int main(void) {
    /* 1. class-set shape + UTF-8 encoding of accented glyphs. */
    CK(OCR_ENGLISH_N == 95, "English tier has 95 classes");
    CK(OCR_LATIN1_N == 96, "Latin-1 tier has 96 classes");
    CK(ocr_is_latin1("\u00e9"), "e-acute is a Latin-1 member");
    /* U+00A0 is the first Latin-1 glyph (index 0) -> UTF-8 C2 A0. */
    CK(OCR_LATIN1_CHARS[0][0] == (char)0xC2 && OCR_LATIN1_CHARS[0][1] == (char)0xA0,
       "Latin-1[0] (U+00A0) is 2-byte UTF-8");
    /* e-acute = U+00E9 -> UTF-8 C3 A9, at Latin-1 index 0xE9-0xA0 = 73. */
    {
        size_t eidx = 0xE9 - 0xA0;
        CK(OCR_LATIN1_CHARS[eidx][0] == (char)0xC3 && OCR_LATIN1_CHARS[eidx][1] == (char)0xA9,
           "Latin-1 e-acute (U+00E9) is 2-byte UTF-8 (C3 A9)");
    }

    const void *fonts[OCR_FONTBANK_MAX];
    size_t nfonts = 0;
    uint8_t *bufs[OCR_FONTBANK_MAX];
    Font *fobjs[OCR_FONTBANK_MAX];
    for (int i = 0; candidate_fonts[i] && nfonts < OCR_FONTBANK_MAX; i++) {
        size_t n = 0;
        uint8_t *b = slurp(candidate_fonts[i], &n);
        if (!b) continue;
        Font *fo = font_open(b, n);
        if (!fo) { free(b); continue; }
        bufs[nfonts] = b; fobjs[nfonts] = fo; fonts[nfonts] = fo; nfonts++;
    }
    if (nfonts == 0) { printf("SKIP: no system font found\n"); return 0; }
    printf("using %zu font(s) for Latin tier\n", nfonts);

    /* 2. composite English+Latin bank reads accented glyphs. */
    const char *classes[OCR_ENGLISH_N + OCR_LATIN1_N + 1];
    size_t nc = ocr_classes_english_latin(classes);
    classes[nc] = NULL;
    CK(nc == 191, "composite class set has 191 classes");

    OcrFontBank *bank = ocr_fontbank_build(fonts, nfonts, 5, 48, classes);
    CK(bank != NULL, "composite bank builds");
    if (!bank) {
        for (size_t i = 0; i < nfonts; i++) { font_free(fobjs[i]); free(bufs[i]); }
        return 1;
    }

    /* compose a warped Latin-only crowd and OCR it; accented glyphs must be
     * read (recall > 0). */
    double rec = ocr_gauntlet_scatter(bank, (const Font *const *)fobjs, nfonts,
                                      OCR_LATIN1_CHARS, OCR_LATIN1_N,
                                      512, 512, 48, 7007, 8.0, 0.2, 5.0);
    CK(rec >= 0.0 && rec <= 1.0, "latin1 scatter recall in [0,1]");
    CK(rec > 0.0, "latin1 bank reads accented glyphs (recall > 0)");
    printf("  latin1 scatter recall (read/placed) = %.1f%%\n", 100.0 * rec);

    ocr_fontbank_free(bank);
    for (size_t i = 0; i < nfonts; i++) { font_free(fobjs[i]); free(bufs[i]); }

    if (fails) { printf("LATIN1 TESTS FAILED (%d)\n", fails); return 1; }
    printf("LATIN1 TESTS PASSED (%zu fonts)\n", nfonts);
    return 0;
}
