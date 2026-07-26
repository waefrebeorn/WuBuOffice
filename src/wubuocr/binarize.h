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

/* Sauvola adaptive threshold MAP: returns a w*h array of per-pixel thresholds
 * (dark <= T == ink). win = window radius*2+1 (e.g. 15-31 for scans), k ~ 0.2-0.5,
 * R = dynamic range constant (~128). NULL on OOM/empty input. Caller frees. */
uint8_t *ocr_sauvola_thresh(const OcrImage *im, int win, double k, double R);

/* Opaque 1-bit-per-pixel ink map: bit set == foreground (dark) pixel. */
typedef struct OcrBinary OcrBinary;

/* Binarize `im` at the given threshold (use ocr_otsu_threshold to pick it, or
 * pass a fixed value). NULL on OOM/empty input. */
OcrBinary *ocr_binarize(const OcrImage *im, uint8_t threshold);

/* Binarize `im` with a per-pixel Sauvola adaptive threshold map. NULL on error. */
OcrBinary *ocr_binarize_sauvola(const OcrImage *im, int win, double k, double R);

/* Remove isolated foreground specks (1-opening): a foreground bit with no
 * 8-connected foreground neighbour is treated as noise and cleared. */
void ocr_binary_despeckle(OcrBinary *b);

/* Fraction of pixels that are ink (0..1). Used for empty-page detection. */
double ocr_binary_ink_fraction(const OcrBinary *b);

/* Crop `im` to the ink bounding box (+`pad` px margin). NULL if empty/OOM. */
OcrImage *ocr_image_autocrop(const OcrImage *im, const OcrBinary *b, int pad);

void       ocr_binary_free(OcrBinary *b);

size_t ocr_binary_width(const OcrBinary *b);
size_t ocr_binary_height(const OcrBinary *b);

/* 1 if (x,y) is foreground ink, else 0. Out-of-range -> 0 (background). */
int ocr_binary_ink(const OcrBinary *b, size_t x, size_t y);

/* Build an ink map directly from a raw pixel plane (row-major, w*h bytes,
 * 0=black..255=white). Bits with grayscale <= `ink_threshold` are treated as
 * foreground ink. Used to wrap dataset glyphs (e.g. EMNIST 28x28) without an
 * OcrImage round-trip. NULL on OOM/empty input. The `bits` buffer is copied. */
OcrBinary *ocr_binary_from_raw(const uint8_t *px, size_t w, size_t h,
                               uint8_t ink_threshold);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_BINARIZE_H */
