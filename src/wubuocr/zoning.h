/* zoning.h -- principled, interpretable glyph feature extraction.
 *
 * Opaque extractor: the caller sees only ZoningExtractor and a fixed feature
 * vector z[]; the internal grid/structure math is hidden. Self-contained
 * dependency-free C11 (no WuBu or OCR deps).
 *
 * The feature vector captures the SHAPE of a glyph independently of size and
 * translation:
 *   [0 .. grid*grid-1]  ink-fraction per cell of an NxN grid over the tight
 *                       ink bounding box (scale/translation invariant)
 *   [grid*grid]         aspect ratio (width/height) of the ink bounding box
 *   [grid*grid+1]       number of holes (enclosed background regions)
 *   [grid*grid+2..+5]   ink fraction in each of the 4 quadrants
 *   [grid*grid+6..+7]   center-of-mass (x,y) within the bounding box, in [0,1]
 *
 * EMNIST pixels arrive as 0=bg,255=ink; callers pass an INVERTED plane where
 * ink is DARK (<=127) so the same extractor works for any source.
 */
#ifndef WUBUOCR_ZONING_H
#define WUBUOCR_ZONING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZoningExtractor ZoningExtractor;

/* Create an extractor for the given zoning grid resolution. */
ZoningExtractor *zoning_create(int grid);
void zoning_destroy(ZoningExtractor *z);

/* Feature dimension produced by this extractor. */
int zoning_dim(const ZoningExtractor *z);

/* Extract features for a w x h binary-ish plane (ink DARK, i.e. <=127).
 * Writes exactly zoning_dim(z) floats into out[]. Returns the dim. */
int zoning_extract(const ZoningExtractor *z, const unsigned char *px,
                   int w, int h, float *out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_ZONING_H */
