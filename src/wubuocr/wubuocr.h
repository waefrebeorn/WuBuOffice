/* wubuocr.h -- image -> structured document digestion facade.
 *
 * Ties the pipeline together: raster page (Netpbm) -> Otsu binarize -> XY-cut
 * layout (reading order) -> per-block connected components (glyph boxes) ->
 * a structured document model serialized as JSON that the rest of WuBuOffice
 * already knows how to emit (docx/odt/md/html) via wubudoc/wubuconv.
 *
 * The single learned step -- glyph pixels to Unicode text -- is isolated behind
 * a pluggable OcrRecognizer callback. WuBuOCR NEVER fabricates text: with no
 * recognizer installed, glyph geometry is reported with empty text, honestly.
 * A real classifier (neural or feature-based) drops into the slot without
 * touching the deterministic pipeline. This is exactly the boundary DeepSeek-OCR
 * / Nemotron-Parse blur inside one trained model; here it is explicit.
 *
 * Opaque page handle; JSON model owned by caller (free with free()).
 */
#ifndef WUBUOCR_H
#define WUBUOCR_H

#include <stddef.h>
#include <stdint.h>
#include "image.h"
#include "binarize.h"
#include "layout.h"
#include "components.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Recognizer plug-in: given the ink map and a single glyph's bounding box,
 * return a malloc'd UTF-8 string for that glyph (caller frees), or NULL if it
 * cannot classify it. `user` is the opaque context passed to ocr_page_analyze.
 * A NULL recognizer means "geometry only" -- no text is invented. */
typedef char *(*OcrRecognizer)(const OcrBinary *b, const OcrBlock *glyph,
                               void *user);

typedef struct OcrPage OcrPage;

/* Analyze a decoded image into a structured page. `rec`/`user` may be NULL
 * (geometry-only). `params` may be NULL (auto layout tuning). NULL on failure. */
OcrPage *ocr_page_analyze(const OcrImage *im, const OcrLayoutParams *params,
                          OcrRecognizer rec, void *user);

/* Convenience: decode a Netpbm blob and analyze it in one call. */
OcrPage *ocr_page_from_netpbm(const uint8_t *data, size_t len,
                              OcrRecognizer rec, void *user);

void ocr_page_free(OcrPage *pg);

/* Number of reading-order blocks detected. */
size_t          ocr_page_block_count(const OcrPage *pg);
const OcrBlock *ocr_page_block(const OcrPage *pg, size_t i);

/* Recognized text of the i-th reading-order block (concatenated glyph
 * text; "" if geometry-only). Returns an internal NUL-terminated
 * string owned by the page (caler must NOT free it). NULL if
 * out of range. Use this instead of scraping the JSON when you
 * need structured block text. */
const char *ocr_page_block_text(const OcrPage *pg, size_t i);

/* Per-glyph box accessors: true document coordinates of each connected-
 * component glyph inside reading-order block `bi`. NULL/out-of-range safe. */
size_t          ocr_page_glyph_count(const OcrPage *pg, size_t bi);
const OcrBlock *ocr_page_glyph(const OcrPage *pg, size_t bi, size_t k);

/* Serialize the page as a JSON model:
 *   {"type":"ocr_page","width":W,"height":H,"threshold":T,
 *    "blocks":[{"x":..,"y":..,"w":..,"h":..,"order":i,
 *               "glyphs":[{"x":..,"y":..,"w":..,"h":..,"text":"..."}],
 *               "text":"concatenated glyph text"}]}
 * Returns a malloc'd NUL-terminated string (caller frees), NULL on failure. */
char *ocr_page_to_json(const OcrPage *pg);

/* Serialize as a wubudoc "document" model (paragraph blocks), so the OCR result
 * feeds straight into doc create (json -> docx/odt/md/...). One paragraph per
 * layout block, text = recognized glyph text (empty if geometry-only).
 * Returns malloc'd JSON (caller frees), NULL on failure. */
char *ocr_page_to_docmodel_json(const OcrPage *pg);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_H */
