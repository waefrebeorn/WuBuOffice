/* wubugauntlet_cli -- OCR robustness battery for the multi-font bank.
 *
 * Usage:
 *   wubugauntlet_cli [--ppm N] [--text "Hello"]
 *
 * Loads every system font it can open, builds the multi-font bank,
 * then runs the gauntlet: for each corruption operator (2D rotation,
 * 3D-ish perspective, DFT low-pass, JPEG-like block quantization)
 * it sweeps a severity range, renders the probe text from a held-out
 * font, OCRs through the bank, and prints accuracy per step.
 * Finally it prints a FONT ABLATION table: rebuilding the bank
 * with each font dropped, showing how much accuracy the held-out
 * probe loses -- the empirical payoff of "study many font types".
 *
 * All corruptions are deterministic and dependency-free (naive DFT,
 * bilinear resample). No model, no training.
 */
#include "fontbank.h"
#include "wubufont.h"
#include "gauntlet.h"

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
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc) ppm = atoi(argv[++i]);
        else if (strcmp(argv[i], "--text") == 0 && i + 1 < argc) text = argv[++i];
    }

    /* load fonts */
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
    if (nf == 0) { fprintf(stderr, "error: no system font found\n"); return 1; }
    printf("loaded %zu font(s) for the bank\n", nf);

    OcrFontBank *bank = ocr_fontbank_build(fonts, nf, 5, ppm);
    if (!bank) { fprintf(stderr, "error: bank build failed\n"); return 1; }

    /* severity sweeps per operator */
    double steps[6];
    GOp ops[4] = { GA_ROTATE, GA_PERSPECTIVE, GA_DFT_LOWPASS, GA_BLOCK_QUANT };
    for (int o = 0; o < 4; o++) {
        GOp op = ops[o];
        /* choose a meaningful severity range per op */
        int nstep = 0;
        if (op == GA_ROTATE)      { for (int i=0;i<6;i++) steps[nstep++] = -15 + 6*i; }      /* -15..15 deg */
        else if (op == GA_PERSPECTIVE) { for (int i=0;i<6;i++) steps[nstep++] = 0.1 + 0.08*i; } /* 0.1..0.5 */
        else if (op == GA_DFT_LOWPASS) { for (int i=0;i<6;i++) steps[nstep++] = 0.15 + 0.15*i; } /* keep 0.15..0.9 */
        else                      { for (int i=0;i<6;i++) steps[nstep++] = 4 + 6*i; }      /* quant step 4..34 */
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
               i, 100.0 * a, 100.0 * (a - 1.0)); /* vs 1.0 = full bank at sev 0 */
    }

    ocr_fontbank_free(bank);
    for (size_t i = 0; i < nf; i++) { font_free(objs[i]); free(bufs[i]); }
    return 0;
}
