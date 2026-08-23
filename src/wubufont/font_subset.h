/* font_subset.h -- N1: sfnt glyph subsetting for PDF embedding. */
#ifndef WUBUFONT_SUBSET_H
#define WUBUFONT_SUBSET_H

#include "wubufont.h"
#include <stddef.h>
#include <stdint.h>

/* Subset `f` to only the glyphs reachable from `codepoints` (via cmap +
 * composite closure). Emits a valid sfnt (glyf/loca/hmtx/cmap rebuilt,
 * other tables copied). Returns 0 and a malloc'd blob in *out, or -1. */
int wubufont_subset(Font *f, const uint32_t *codepoints, size_t ncodes,
                    uint8_t **out, size_t *out_len);

#endif
