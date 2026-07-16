/* fontdir.h -- discover and load a whole directory of fonts.
 *
 * The user's "massive collection of fonts makes this super effective": the
 * OCR bank's strength scales with how many real fonts it can vote across, so
 * we need to load fonts by directory rather than a hardcoded candidate list.
 * This module walks a directory for files with a .ttf/.otf/.ttc extension,
 * it with the clean-room wubufont rasterizer. The caller owns the returned
 * buffers + Font objects and frees them with ocr_font_dir_free.
 */
#ifndef WUBUOCR_FONTDIR_H
#define WUBUOCR_FONTDIR_H

#include <stddef.h>

#include "wubufont.h"   /* Font */

/* Discover fonts under `dir` (recursive, depth-bounded). Opens up to `max`
 * of them. On success returns the count opened (0..max) and fills:
 *   *out_fonts : array of `count` opaque Font* (borrowed buffers, kept alive)
 *   *out_bufs  : array of `count` malloc'd file buffers (free these last)
 *   *out_paths : array of `count` malloc'd path strings
 * The caller must free everything via ocr_font_dir_free. Returns 0 and sets
 * outputs to NULL on error / empty directory. */
size_t ocr_font_dir_load(const char *dir, Font ***out_fonts, uint8_t ***out_bufs,
                         char ***out_paths, size_t max);

/* Free the three arrays returned by ocr_font_dir_load, closing `count` fonts. */
void ocr_font_dir_free(Font **fonts, uint8_t **bufs, char **paths, size_t count);

#endif /* WUBUOCR_FONTDIR_H */
