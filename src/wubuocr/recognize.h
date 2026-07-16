/* recognize.h -- lightest-weight glyph recognizer: zoning + 1-NN templates.
 *
 * The recognizer plug-in for the wubuocr pipeline. This is the classic
 * *smallest possible* OCR classifier -- no neural net, no training loop, no
 * external model:
 *
 *   1. ZONING (feature extraction): scale the candidate glyph's bounding box
 *      into an NxN grid and compute the ink-density of each cell -> an N*N
 *      feature vector. Zoning is the canonical lightweight OCR feature (it is
 *      scale-invariant and cheap: O(pixels) once).
 *   2. 1-NEAREST-NEIGHBOUR: compare that vector against a template vector for
 *      each reference glyph (rasterized from the embedded font8x8) using
 *      squared Euclidean distance, and return the closest match. k=1 KNN is the
 *      simplest possible classifier and needs no training beyond building the
 *      templates once.
 *
 * It recognizes the printable ASCII range (U+0020..U+007E). A distance-ratio
 * confidence gate rejects ambiguous blobs (returns no character) so garbage is
 * never turned into fabricated text.
 */
#ifndef WUBUOCR_RECOGNIZE_H
#define WUBUOCR_RECOGNIZE_H

#include <stddef.h>
#include "binarize.h"
#include "layout.h"
#include "wubuocr.h"   /* OcrRecognizer typedef */

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque recognizer state (holds the precomputed zoning templates). */
typedef struct OcrTemplates OcrTemplates;

/* Build the template bank from the embedded font8x8 using an NxN zoning grid
 * (typical N=4 or 5). Returns NULL on OOM or invalid grid. Caller frees with
 * ocr_templates_free(). Covers the printable ASCII range (U+0020..U+007E). */
OcrTemplates *ocr_templates_create(size_t grid);

/* Build an EMPTY template bank for an arbitrary class set: `classes` points
 * at `nclass` consecutive UTF-8 chars (1 byte each). Fill it by accumulating
 * training samples with ocr_templates_add_sample(), then ocr_templates_finalize().
 * Used to TRAIN the recognizer on a labeled dataset (e.g. EMNIST Letters). */
OcrTemplates *ocr_templates_create_classes(size_t grid, const char *classes,
                                            size_t nclass);

/* Accumulate one labeled training glyph (raw grayscale plane, row-major,
 * 0=black..255=white; ink where pixel <= ink_threshold) into class index
 * `class_idx`. The glyph's tight ink bounding box is zoned, so the template is
 * scale/translation invariant like a candidate. Call ocr_templates_finalize()
 * after all samples to convert the accumulators into mean zoning vectors. */
void ocr_templates_add_sample(OcrTemplates *t, size_t class_idx,
                              const uint8_t *px, size_t w, size_t h,
                              uint8_t ink_threshold);

/* Finalize after accumulation: divide each class accumulator by its sample
 * count to produce the mean zoning template. Classes with zero samples keep a
 * zero vector (will never win). Safe to call once; subsequent calls are no-ops. */
void ocr_templates_finalize(OcrTemplates *t);

/* Tune / disable the confidence gate. enabled=0 -> closed-set mode (always
 * return the nearest class; correct for a benchmark where every glyph is one
 * of the classes). enabled=1 keeps the gate: best must be within
 * ratio*second AND abs*dim of the nearest template, else rejected (NULL). */
void ocr_templates_set_reject(OcrTemplates *t, int enabled,
                              double ratio, double abs);

/* Enable structural augmentation in the 1-NN distance: aspect-ratio mismatch
 * weighted by `ar_w` and a hole-count mismatch by `hole_w`. (ar_w=hole_w=0 ->
 * pure zoning.) Call after finalize(), before recognition. */
void ocr_templates_set_struct(OcrTemplates *t, double ar_w, double hole_w);

/* Read back a class's trained structural features (mean aspect ratio, mean
 * hole count). Useful for diagnostics; returns 0 if idx out of range. */
int ocr_templates_struct_info(const OcrTemplates *t, size_t idx,
                              double *out_ar, int *out_holes);

void ocr_templates_free(OcrTemplates *t);

/* An OcrRecognizer (see wubuocr.h) bound to a template bank: pass the
 * OcrTemplates* as the `user` pointer to ocr_page_analyze. Classifies one glyph
 * box and returns a malloc'd 1-char UTF-8 string, or NULL if below the
 * confidence gate. */
char *ocr_recognize_glyph(const OcrBinary *b, const OcrBlock *glyph, void *user);

/* Convenience accessor so callers can wire the pair in one place. */
OcrRecognizer ocr_recognizer_fn(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_RECOGNIZE_H */
