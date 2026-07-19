/* goldplace.h -- golden-ratio coordinate placement for multi-style OCR pages.
 *
 * Reuses the WuBuMath GAAD golden-ratio machinery
 * (generate_phi_spiral / golden_subdivide) to place glyphs at GOLDEN-RATIO
 * coordinates instead of uniform-random scatter. This is the "warped
 * coordinate ablation on multi-coordinate styles" idea: each glyph is dropped
 * at a point sampled from the golden (phi) spiral AND inside a golden-subdivided
 * region, so the coverage is:
 *   - multi-scale (golden subdivision -> regions follow PHI aspect splits),
 *   - non-gridded (phi spiral -> no Moire / no alignment artifacts),
 *   - resolution-agnostic (the same spiral/subdivision rules apply at any ppm,
 *     which is exactly the "agnostic anti-aliasing" property: coordinates do
 *     not depend on raster resolution).
 *
 * This module is a thin adapter: it copies the two proven WuBuMath functions'
 * behavior locally (NOT linking WuBuMath at OCR-build time) so the OCR stack
 * stays dependency-free per its 2011 single-core framing. The PHI constant and
 * the subdivision/spirial math are byte-identical to WuBuMath/src/model/
 * wubu_gaad_encoder.c (kept in sync intentionally).
 */
#ifndef WUBUOCR_GOLDPLACE_H
#define WUBUOCR_GOLDPLACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GOLDPLACE_PHI 1.6180339887498948482
#define GOLDPLACE_LOG_PHI_2 (0.48121182505960347)

/* A golden-ratio sample point on a W x H canvas. */
typedef struct { double x, y; } GoldPoint;

/* A golden-subdivided rectangular region (page coordinates). */
typedef struct { double x1, y1, x2, y2; } GoldRect;

/* Fill `pts` (capacity npts) with phi-spiral sample points over a WxH canvas.
 * Returns the number written (<= npts). Deterministic. */
size_t goldplace_spiral(GoldPoint *pts, size_t npts, int W, int H);

/* Fill `rects` (capacity nrects) with golden-subdivided regions of a WxH
 * canvas (target ~nrects leaf regions). Returns the number written. */
size_t goldplace_subdivide(GoldRect *rects, size_t nrects, int W, int H);

/* Place `nglyph` glyphs: pick golden-subdivided region `i` (round-robin),
 * then a phi-spiral point inside it, write the box center into `cx,cy`
 * (caller arrays of size nglyph). Deterministic for a given (W,H). This is the
 * multi-coordinate-style ablation: glyphs inherit (region_index, spiral_index)
 * tags the ingestion can read back. Returns number placed. */
size_t goldplace_layout(int W, int H, size_t nglyph, double *cx, double *cy,
                        int *region_idx, int *spiral_idx);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_GOLDPLACE_H */
