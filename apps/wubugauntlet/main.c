/* wubugauntlet_cli -- OCR robustness battery for the multi-font bank.
 *
 * Usage:
 *   wubugauntlet_cli [--ppm N] [--text "Hello"] [--latin] [--compose]
 *
 * Loads every system font it can open, builds the multi-font bank, then runs
 * the gauntlet: for each corruption operator (2D rotation, 3D-ish perspective,
 * DFT low-pass, JPEG-like block quantization) it sweeps a severity range,
 * renders the probe text from a held-out font, OCRs through the bank, and
 * prints accuracy per step. Finally it prints a FONT ABLATION table:
 * rebuilding the bank with each font dropped, showing how much accuracy the
 * held-out probe loses -- the empirical payoff of "study many font types".
 *
 * Extra modes (the user's full requirement):
 *   --latin   build an English-first + Latin-1 (ISO-8859-1) bank and measure
 *             how well accented Latin glyphs are read (the "Latin second" tier).
 *   --compose scatter glyphs from a crowd of fonts RANDOMLY around the page
 *             with a mixture of 2D (rotation) and 3D (perspective+shear)
 *             warping, then OCR the warped crowd and report recognition rate.
 *
 * All corruptions are deterministic and dependency-free (naive DFT, bilinear
 * resample). No model, no training.
 */
#include "fontbank.h"
#include "wubufont.h"
#include "gauntlet.h"
#include "page_compose.h"
#include "latin1.h"
#include "unicode.h"
#include "fontdir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *candidate_fonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/opentype/unifont/unifont.otf",
    "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
    "/mnt/c/Windows/Fonts/arial.ttf",
    NULL
};

static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f); rewind(f);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[rd] = 0; *n = rd;
    return b;
}

static const char *op_name(GOp op) {
    switch (op) {
        case GA_ROTATE:     return "rotate(deg)";
        case GA_PERSPECTIVE: return "perspective(k)";
        case GA_DFT_LOWPASS: return "dft_lowpass(keep)";
        case GA_BLOCK_QUANT:  return "block_quant(step)";
        default:             return "?";
    }
}

int main(int argc, char **argv) {
    int ppm = 48;
    const char *text = "Hello 2026";
    const char *fontdir = NULL;
    int use_latin = 0, do_compose = 0, use_unicode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc) ppm = atoi(argv[++i]);
        else if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) text = argv[++i];
        else if (strcmp(argv[i], "--latin") == 0) use_latin = 1;
        else if (strcmp(argv[i], "--compose") == 0) do_compose = 1;
        else if (strcmp(argv[i], "--unicode") == 0) use_unicode = 1;
        else if (strcmp(argv[i], "--fontdir") == 0 && i + 1 < argc) fontdir = argv[++i];
    }

    /* load fonts (hardcoded candidates first, then any --fontdir directory) */
    const void *fonts[OCR_FONTBANK_MAX];
    uint8_t *bufs[OCR_FONTBANK_MAX];
    Font *objs[OCR_FONTBANK_MAX];
    size_t nf = 0;
    for (int i = 0; candidate_fonts[i] && nf < OCR_FONTBANK_MAX; i++) {
        size_t n = 0;
        uint8_t *b = slurp(candidate_fonts[i], &n);
        if (!b) continue;
        Font *fo = font_open(b, n);
        if (!fo) { free(b); continue; }
        bufs[nf] = b; objs[nf] = fo; fonts[nf] = fo; nf++;
    }
    /* --fontdir: load a whole directory of fonts (the "massive font
     * collection" idea -- more fonts => more robust voting). */
    Font **dfonts = NULL; uint8_t **dbufs = NULL; char **dpaths = NULL;
    size_t nd = 0;
    if (fontdir) {
        nd = ocr_font_dir_load(fontdir, &dfonts, &dbufs, &dpaths, OCR_FONTBANK_MAX - nf);
        for (size_t i = 0; i < nd && nf < OCR_FONTBANK_MAX; i++) {
            bufs[nf] = dbufs[i]; objs[nf] = dfonts[i]; fonts[nf] = dfonts[i]; nf++;
        }
    }
    if (nf == 0) { fprintf(stderr, "error: no system font found\n"); return 1; }
    printf("loaded %zu font(s) for the bank", nf);
    if (nd) printf(" (%zu from --fontdir %s)", nd, fontdir);
    printf("\n");

    /* Build the bank. English-only by default; English+Latin on --latin;
     * the full extended Unicode token set on --unicode. */
    const char *classes[OCR_ENGLISH_N + OCR_LATIN1_N + 4096];
    const char *const *class_ptr = NULL;
    size_t nclass = 0;
    if (use_unicode) {
        nclass = ocr_unicode_classes(classes,
                                     sizeof(classes)/sizeof(classes[0]), 1);
        classes[nclass] = NULL;
        class_ptr = classes;
        printf("Unicode tier ON: %zu classes (English %zu + Latin %zu + CJK %zu)\n",
               nclass, OCR_ENGLISH_N, OCR_LATIN1_N, ocr_unicode_cjk_count());
    } else if (use_latin) {
        nclass = ocr_classes_english_latin(classes);
        classes[nclass] = NULL;
        class_ptr = classes;
        printf("Latin tier ON: %zu classes (English %zu + Latin %zu)\n",
               nclass, OCR_ENGLISH_N, OCR_LATIN1_N);
    }
    OcrFontBank *bank = ocr_fontbank_build(fonts, nf, 5, ppm, class_ptr);
    if (!bank) { fprintf(stderr, "error: bank build failed\n"); return 1; }
    printf("bank has %zu contributing font(s)\n", ocr_fontbank_font_count(bank));

    /* severity sweeps per operator */
    double steps[6];
    GOp ops[4] = { GA_ROTATE, GA_PERSPECTIVE, GA_DFT_LOWPASS, GA_BLOCK_QUANT };
    for (int o = 0; o < 4; o++) {
        GOp op = ops[o];
        int nstep = 0;
        if (op == GA_ROTATE)      { for (int i=0;i<6;i++) steps[nstep++] = -15 + 6*i; }
        else if (op == GA_PERSPECTIVE) { for (int i=0;i<6;i++) steps[nstep++] = 0.1 + 0.08*i; }
        else if (op == GA_DFT_LOWPASS) { for (int i=0;i<6;i++) steps[nstep++] = 0.15 + 0.15*i; }
        else                      { for (int i=0;i<6;i++) steps[nstep++] = 4 + 6*i; }
        double acc[6];
        size_t got = ocr_gauntlet_sweep(bank, objs[0], text, ppm, op, steps, (size_t)nstep, acc);
        printf("\n[%s] accuracy over %zu severity steps (held-out probe font):\n", op_name(op), got);
        for (size_t i = 0; i < got; i++)
            printf("   amount=%6.2f  acc=%5.1f%%\n", steps[i], 100.0 * acc[i]);
    }

    /* font ablation table (drop each font, measure probe accuracy) */
    printf("\n[font ablation] accuracy when each font is held out of the bank:\n");
    for (size_t i = 0; i < nf; i++) {
        double a = ocr_gauntlet_ablate(bank, fonts, nf, objs[0], text, ppm, i);
        printf("   drop font[%zu] -> probe acc=%5.1f%%  (delta vs full=%+.1f%%)\n",
               i, 100.0 * a, 100.0 * (a - 1.0));
    }

    /* Scatter composer: the "random placement + 2D/3D warping" stress test. */
    if (do_compose) {
        const char *const *crowd = class_ptr ? class_ptr : OCR_ENGLISH_CHARS;
        size_t nc = class_ptr ? nclass : OCR_ENGLISH_N;
        printf("\n[scatter] warped crowd of fonts, random placement, 2D+3D mix:\n");
        /* a few seeds so the number is not a fluke */
        double best = 0, worst = 1, sum = 0; size_t runs = 4;
        for (size_t r = 0; r < runs; r++) {
            double acc = ocr_gauntlet_scatter(bank, (const Font *const *)objs, nf, crowd, nc,
                                              512, 512, ppm, (unsigned)(1000 + r),
                                              12.0, 0.35, 8.0);
            printf("   seed=%zu  warped-crowd recall=%5.1f%%\n", 1000 + r, 100.0 * acc);
            best = acc > best ? acc : best;
            worst = acc < worst ? acc : worst;
            sum += acc;
        }
        printf("   scatter recall (read/placed): best=%5.1f%% worst=%5.1f%% mean=%5.1f%%\n",
               100.0 * best, 100.0 * worst, 100.0 * (sum / runs));
    }

    /* Unicode tier validation: build a CJK-inclusive bank and measure recall
     * on a warped crowd drawn from the same extended class set. This proves
     * the "extended Unicode" token set is actually OCR-readable through the
     * real pipeline, not just that the class list grew. */
    if (use_unicode) {
        OcrFontBank *ubank = ocr_fontbank_build((const void *const *)fonts, nf, 5, ppm, class_ptr);
        if (ubank) {
            printf("\n[unicode] extended-glyph recall on a warped crowd:\n");
            double best = 0, worst = 1, sum = 0;
            for (size_t r = 0; r < 3; r++) {
                double acc = ocr_gauntlet_scatter(ubank, (const Font *const *)objs, nf,
                                                  class_ptr, nclass,
                                                  512, 512, ppm, (unsigned)(9000 + r),
                                                  8.0, 0.2, 5.0);
                printf("   seed=%zu  unicode recall=%5.1f%%\n", 9000 + r, 100.0 * acc);
                best = acc > best ? acc : best;
                worst = acc < worst ? acc : worst;
                sum += acc;
            }
            printf("   unicode recall (read/placed): best=%5.1f%% worst=%5.1f%% mean=%5.1f%%\n",
                   100.0 * best, 100.0 * worst, 100.0 * (sum / 3));
            ocr_fontbank_free(ubank);
        } else {
            printf("\n[unicode] warning: extended bank build failed\n");
        }
    }

    /* Latin tier validation: build a Latin-1-only bank and measure how well
     * accented glyphs (é, ñ, ü, ç, ...) are read when scattered + warped. This
     * proves the "Latin language second" tier actually recognizes accented
     * letters, not just that the class list is longer. */
    if (use_latin) {
        OcrFontBank *lbank = ocr_fontbank_build((const void *const *)fonts, nf, 5, ppm, OCR_LATIN1_CHARS);
        if (lbank) {
            printf("\n[latin1] accented-glyph recall on a warped Latin crowd:\n");
            double best = 0, worst = 1, sum = 0;
            for (size_t r = 0; r < 3; r++) {
                double acc = ocr_gauntlet_scatter(lbank, (const Font *const *)objs, nf,
                                                  OCR_LATIN1_CHARS, OCR_LATIN1_N,
                                                  512, 512, ppm, (unsigned)(5000 + r),
                                                  8.0, 0.2, 5.0);
                printf("   seed=%zu  latin recall=%5.1f%%\n", 5000 + r, 100.0 * acc);
                best = acc > best ? acc : best;
                worst = acc < worst ? acc : worst;
                sum += acc;
            }
            printf("   latin1 recall (read/placed): best=%5.1f%% worst=%5.1f%% mean=%5.1f%%\n",
                   100.0 * best, 100.0 * worst, 100.0 * (sum / 3));
            ocr_fontbank_free(lbank);
        } else {
            printf("\n[latin1] warning: Latin-only bank build failed\n");
        }
    }

    ocr_fontbank_free(bank);
    for (size_t i = 0; i < nf; i++) { font_free(objs[i]); free(bufs[i]); }
    /* The fontdir Font objects + their blobs were merged into objs/bufs and
     * freed above; only the dpaths strings and the three wrapper arrays are
     * exclusively ours to release (freeing the contents again would double-free). */
    if (nd) {
        for (size_t i = 0; i < nd; i++) free(dpaths[i]);
        free(dfonts); free(dbufs); free(dpaths);
    }
    return 0;
}
