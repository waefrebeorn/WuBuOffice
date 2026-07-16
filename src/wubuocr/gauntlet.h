/* gauntlet.h -- OCR robustness battery for the multi-font bank.
 *
 * Stress-tests a built OcrFontBank the way real scans stress OCR: it
 * renders a KNOWN string from a probe font, applies a corruption
 * (2D rotation, 3D-ish perspective, DFT low-pass / JPEG-like block
 * quantization as compression-loss proxies), OCRs the corrupted glyphs
 * through the bank, and reports accuracy across a sweep of severity.
 *
 * It also runs FONT ABLATION: rebuild the bank with one font held
 * out and measure the accuracy delta, quantifying each font's marginal
 * contribution to the "study many font types" idea.
 *
 * Everything is deterministic and dependency-free (naive DFT, bilinear
 * resample) so the numbers are reproducible. Opaque handle-free: all
 * primitives are pure functions over OcrImage. Self-contained: depends
 * only on wubuocr (image + fontbank) and wubufont.
 */
#ifndef WUBUOCR_GAUNTLET_H
#define WUBUOCR_GAUNTLET_H

#include <stddef.h>
#include <stdint.h>

#include "image.h"        /* OcrImage */
#include "fontbank.h"    /* OcrFontBank */
#include "wubufont.h"    /* Font (opaque; used as a const pointer) */

/* Corruption operators (the "pain points" of real OCR ingestion). */
typedef enum {
    GA_ROTATE,          /* 2D in-plane rotation (degrees) */
    GA_PERSPECTIVE,     /* 3D-ish trapezoid (k = strength) */
    GA_DFT_LOWPASS,     /* zero high DFT coefficients (keep = fraction) */
    GA_BLOCK_QUANT,      /* JPEG-like 8x8 block quant (q = step) */
    GA_COUNT
} GOp;

/* Apply one corruption to a grayscale image; returns a NEW image the
 * caller frees with ocr_image_free(), or NULL on OOM/empty input. */
OcrImage *ocr_gauntlet_apply(const OcrImage *im, GOp op, double amount);

/* Render `text` from `probe` at `ppm`, apply `op` at each `amounts[i]`,
 * OCR through `bank`, and fill `acc[i]` (0..1) = fraction of glyphs
 * whose recognized char equals the source. Returns the number of steps
 * written (<= n). `acc` may be NULL (dry run / just validate). */
size_t ocr_gauntlet_sweep(const OcrFontBank *bank, const Font *probe,
                             const char *text, int ppm, GOp op,
                             const double *amounts, size_t n, double *acc);

/* Font ablation: rebuild `bank` with the k-th contributing font held
 * out, render `text` from `probe`, and return accuracy of the
 * reduced bank (0..1). Smaller number = that font mattered more. */
double ocr_gauntlet_ablate(const OcrFontBank *bank,
                            const void *const *fonts, size_t nfonts,
                            const Font *probe, const char *text,
                            int ppm, size_t drop_index);

#endif /* WUBUOCR_GAUNTLET_H */
