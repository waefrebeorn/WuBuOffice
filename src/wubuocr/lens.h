/* lens.h -- Office-Lens-style document flattening.
 *
 * The user (or an auto-detector) picks the 4 corner points of a document in a
 * photo (any order). We:
 *   1. reorder the 4 corners to TL,TR,BR,BL,
 *   2. choose a clean output rectangle (aspect from the quad),
 *   3. solve the 3x3 homography mapping output rect -> source quad,
 *   4. inverse-warp (sample source bilinearly) to produce a flat, de-skewed
 *      grayscale page,
 *   5. auto-level: contrast stretch / grayscale flatten.
 *
 * Pure scalar C11, no deps. Homography solve = 8x8 linear system via Gaussian
 * elimination (small, exact enough for corner mapping).
 */
#ifndef WUBUOCR_LENS_H
#define WUBUOCR_LENS_H
#include "image.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { double x, y; } Pt2;

/* Pick 4 corners (any order). out_w/out_h suggested output size (0,0 = derive
 * from quad aspect at 1px/unit). contrast!=0 enables auto stretch. Returns a NEW
 * OcrImage (grayscale, 0=black), or NULL on degenerate quad. */
OcrImage *lens_flatten(const OcrImage *src, Pt2 corners[4],
                       int out_w, int out_h, int contrast);

#ifdef __cplusplus
}
#endif
#endif
