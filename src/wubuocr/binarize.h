/* binarize.h -- WuBuOCR page binarization (grayscale -> ink/background).
 *
 * Otsu's method (1979): choose the global threshold that maximizes
 * between-class variance of the 256-bin intensity histogram. Deterministic,
 * parameter-free, and robust to the uneven exposure that breaks fixed-threshold
 * OCR on real scans (pain point: noise / imperfect scans). The result is a
 * binary "ink map": 1 where a pixel is foreground (dark), 0 for background.
 */
#ifndef WUBUOCR_BINARIZE_H
#define WUBUOCR_BINARIZE_H

#include <stddef.h>
#include <stdint.h>
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the Otsu threshold (0..255) for an image. Pixels with grayscale
 * value <= threshold are ink (foreground). Returns 128 for a NULL/empty image. */
uint8_t ocr_otsu_threshold(const OcrImage *im);

/* Opaque 1-bit-per-pixel ink map: bit set == foreground (dark) pixel. */
typedef struct OcrBinary OcrBinary;

/* Binarize `im` at the given threshold (use ocr_otsu_threshold to pick it, or
 * pass a fixed value). NULL on OOM/empty input. */
OcrBinary *ocr_binarize(const OcrImage *im, uint8_t threshold);
void       ocr_binary_free(OcrBinary *b);

size_t ocr_binary_width(const OcrBinary *b);
size_t ocr_binary_height(const OcrBinary *b);

/* 1 if (x,y) is foreground ink, else 0. Out-of-range -> 0 (background). */
int ocr_binary_ink(const OcrBinary *b, size_t x, size_t y);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_BINARIZE_H */
