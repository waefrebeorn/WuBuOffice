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
 * ocr_templates_free(). */
OcrTemplates *ocr_templates_create(size_t grid);
void          ocr_templates_free(OcrTemplates *t);

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
