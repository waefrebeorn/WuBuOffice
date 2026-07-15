/* layout.h -- WuBuOCR page layout analysis via recursive XY-cut.
 *
 * XY-cut (Nagy & Seth, 1984) is the classical, deterministic algorithm that
 * recovers a page's *reading order* -- the single biggest OCR-digestion pain
 * point (raster-order OCR interleaves multi-column text into gibberish). It
 * recursively splits a region along its widest whitespace "gutter", alternating
 * vertical cuts (which separate columns) and horizontal cuts (which separate
 * paragraphs/lines), until no wide-enough gutter remains. An in-order traversal
 * of the resulting cut tree yields blocks in natural reading order: top-to-
 * bottom within a column, and column-by-column across the page.
 *
 * This is the deterministic, dependency-free ancestor of what DeepSeek-OCR and
 * Nemotron-Parse learned to approximate with a neural encoder. No training,
 * no GPU, no third-party lib.
 */
#ifndef WUBUOCR_LAYOUT_H
#define WUBUOCR_LAYOUT_H

#include <stddef.h>
#include "binarize.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An axis-aligned page region (pixel coordinates, inclusive-exclusive). */
typedef struct {
    size_t x0, y0, x1, y1;   /* [x0,x1) x [y0,y1) */
} OcrBlock;

/* Tunables for the cut heuristics (all in pixels). Pass NULL to ocr_layout for
 * sensible defaults derived from the page size. */
typedef struct {
    size_t min_gutter_v;   /* min blank columns to accept a vertical (column) cut */
    size_t min_gutter_h;   /* min blank rows to accept a horizontal (line) cut */
    size_t min_block_w;    /* stop splitting below this width */
    size_t min_block_h;    /* stop splitting below this height */
} OcrLayoutParams;

/* Opaque, ordered list of leaf blocks in reading order. */
typedef struct OcrLayout OcrLayout;

/* Run recursive XY-cut over the ink map. `params` may be NULL (auto-tune).
 * Returns NULL on OOM/empty input. */
OcrLayout *ocr_layout(const OcrBinary *b, const OcrLayoutParams *params);
void       ocr_layout_free(OcrLayout *L);

/* Number of leaf blocks, and the i-th block in reading order. */
size_t          ocr_layout_count(const OcrLayout *L);
const OcrBlock *ocr_layout_block(const OcrLayout *L, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_LAYOUT_H */
