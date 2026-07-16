/* test_gauntlet.c -- OCR robustness battery (multi-font bank).
 *
 * Loads every available system font, builds the bank, and checks:
 *   1. Each corruption operator (rotate / perspective / DFT low-pass /
 *      block quant) runs without crashing and returns a valid image.
 *   2. At severity 0 (no-op-ish) the bank still reads the probe
 *      text correctly (regression guard for the bank under the gauntlet
 *      harness itself).
 *   3. Font ablation: dropping a contributing font yields a finite
 *      accuracy (the study-many-fonts payoff is measurable).
 * Skips (exit 0) if no system font exists.
 */
#include "gauntlet.h"
#include "fontbank.h"
#include "wubufont.h"
#include "image.h"
#include "page_compose.h"
#include "latin1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static const char *candidate_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/opentype/unifont/unifont.otf",
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
    printf("using %zu font(s)\n", nfonts);

    OcrFontBank *bank = ocr_fontbank_build_english(fonts, nfonts, 5, 48);
    CK(bank != NULL, "bank builds");
    if (!bank) {
        for (size_t i = 0; i < nfonts; i++) { font_free(fobjs[i]); free(bufs[i]); }
        return 1;
    }

    /* 1. each corruption runs and yields a valid image */
    {
        /* render "Hi" inline via wubufont */
        char tst[] = "Hi";
        size_t tn = strlen(tst);
        int ws[8], hs[8]; uint8_t *gs[8];
        int maxh = 0, tot = 0;
        for (size_t i = 0; i < tn; i++) {
            int w = 0, h = 0; uint8_t *bb = NULL;
            if (!font_rasterize(fobjs[0], (uint32_t)tst[i], 48, &bb, &w, &h)) { w=1;h=1;bb=calloc(1,1); }
            gs[i] = bb; ws[i] = w; hs[i] = h; if (h>maxh)maxh=h; tot += w?w:1;
        }
        int g = 12; int W = tot + (int)tn*g, H = maxh + 4;
        OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
        int cx = g;
        for (size_t i = 0; i < tn; i++) {
            int gy = (H - hs[i]) / 2;
            for (int y=0;y<hs[i];y++) for (int x=0;x<ws[i];x++) {
                int px=cx+x, py=gy+y;
                if (px<0||py<0||px>=W||py>=H) continue;
                if (gs[i][(size_t)y*(ws[i]?ws[i]:1)+x]) ocr_image_set(im,(size_t)px,(size_t)py,0);
            }
            free(gs[i]);
            cx += (ws[i]?ws[i]:1) + g;
        }
        GOp ops[4] = { GA_ROTATE, GA_PERSPECTIVE, GA_DFT_LOWPASS, GA_BLOCK_QUANT };
        double amts[4] = { 5.0, 0.2, 0.5, 8.0 };
        for (int o = 0; o < 4; o++) {
            OcrImage *c = ocr_gauntlet_apply(im, ops[o], amts[o]);
            CK(c != NULL, "gauntlet_apply runs");
            CK(c == NULL || (ocr_image_width(c) > 0 && ocr_image_height(c) > 0), "corrupted image valid");
            ocr_image_free(c);
        }
        ocr_image_free(im);

        /* 2. severity-0 sweep on clean probe is still accurate */
        double amts0[1] = { 0.0 };
        double acc[1] = { 0.0 };
        size_t got = ocr_gauntlet_sweep(bank, fobjs[0], "Hello 2026", 48,
                                       GA_ROTATE, amts0, 1, acc);
        CK(got == 1, "sweep returns 1 step");
        CK(acc[0] > 0.5, "severity-0 probe accuracy > 50%");
        printf("  clean-probe accuracy = %.1f%%\n", 100.0 * acc[0]);

        /* 3. font ablation yields a finite accuracy */
        if (nfonts >= 2) {
            double a = ocr_gauntlet_ablate(bank, fonts, nfonts, fobjs[0],
                                                 "Hello 2026", 48, 0);
            CK(a >= 0.0 && a <= 1.0, "ablation accuracy in [0,1]");
            printf("  ablation(drop 0) accuracy = %.1f%%\n", 100.0 * a);
        }

        /* scatter composer: random placement + 2D/3D warping stress test */
        OcrImage *sp = ocr_compose_page((const Font *const *)fobjs, nfonts,
                                        OCR_ENGLISH_CHARS, OCR_ENGLISH_N,
                                        512, 512, 48, 4242, 10.0, 0.3, 6.0, NULL);
        CK(sp != NULL, "compose_page builds a warped crowd");
        if (sp) {
            CK(ocr_image_width(sp) == 512 && ocr_image_height(sp) == 512,
               "composed page is 512x512");
            uint8_t *pgm = NULL; size_t pl = 0;
            CK(ocr_image_to_pgm(sp, &pgm, &pl) == 0 && pgm && pl > 0,
               "composed page serializes to PGM");
            ocr_image_free(sp);
            free(pgm);
            double rec = ocr_gauntlet_scatter(bank, (const Font *const *)fobjs, nfonts,
                                              OCR_ENGLISH_CHARS, OCR_ENGLISH_N,
                                              512, 512, 48, 4242, 10.0, 0.3, 6.0);
            CK(rec >= 0.0 && rec <= 1.0, "scatter recall in [0,1]");
            CK(rec > 0.0, "scatter recall > 0 (bank reads warped glyphs)");
            printf("  scatter recall (read/placed) = %.1f%%\n", 100.0 * rec);
        }
    }

    ocr_fontbank_free(bank);
    for (size_t i = 0; i < nfonts; i++) { font_free(fobjs[i]); free(bufs[i]); }

    if (fails) { printf("GAUNTLET TESTS FAILED (%d)\n", fails); return 1; }
    printf("GAUNTLET TESTS PASSED (%zu fonts)\n", nfonts);
    return 0;
}
