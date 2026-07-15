/* components.h -- connected-component analysis over an ink map.
 *
 * Within a layout block, foreground ink clusters into connected components --
 * typically individual glyphs or glyph fragments. Their bounding boxes are the
 * geometry a recognizer needs, and their spatial grouping recovers words and
 * lines. This is the segmentation layer beneath layout analysis: XY-cut finds
 * the block, components find the glyphs inside it.
 *
 * 8-connectivity, iterative flood fill (explicit stack -- no recursion, so a
 * dense page can't blow the C stack). Restricted to a region so it can run
 * per-block.
 */
#ifndef WUBUOCR_COMPONENTS_H
#define WUBUOCR_COMPONENTS_H

#include <stddef.h>
#include "binarize.h"
#include "layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OcrComponents OcrComponents;

/* Find 8-connected ink components within the region [x0,x1) x [y0,y1) of the
 * ink map. Components smaller than min_area pixels are discarded as noise.
 * Returns NULL on OOM. */
OcrComponents *ocr_components(const OcrBinary *b, size_t x0, size_t y0,
                             size_t x1, size_t y1, size_t min_area);

/* Convenience: run over an OcrBlock. */
OcrComponents *ocr_components_in_block(const OcrBinary *b, const OcrBlock *blk,
                                      size_t min_area);

void ocr_components_free(OcrComponents *c);

size_t          ocr_components_count(const OcrComponents *c);
/* i-th component bounding box (in reading order: top-to-bottom, then
 * left-to-right within a row band). NULL if out of range. */
const OcrBlock *ocr_components_box(const OcrComponents *c, size_t i);
/* i-th component ink pixel count. 0 if out of range. */
size_t          ocr_components_area(const OcrComponents *c, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_COMPONENTS_H */
