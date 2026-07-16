/* page_compose.h -- synthetic document page composer for OCR stress-testing.
 *
 * The user's "study many font types then place them randomly around the page
 * with a mixture of 2-D and 3-D warping" idea: this builds a grayscale OcrImage
 * page littered with glyphs drawn from a CROWD of fonts, each placed at a
 * RANDOM position and warped with a blend of 2D (in-plane rotation) and 3D-ish
 * (perspective trapezoid + shear) transforms. The result drops straight into
 * the real OCR pipeline (ocr_page_from_netpbm), so we can measure how well the
 * multi-font bank reads a genuinely messy page.
 *
 * Two modes:
 *   - ocr_compose_page: novel scatter (random positions + warps + fonts).
 *   - ocr_compose_line: deterministic baseline (one line, no warp) for control.
 *
 * Reuses wubufont for the source glyph bitmaps and the OcrImage API for the
 * page. No new corruption primitives (it composes the same warp math the
 * gauntlet applies to whole pages, per-glyph). Self-contained, opaque-free
 * (pure functions over the Font and OcrImage types). */
#ifndef WUBUOCR_PAGE_COMPOSE_H
#define WUBUOCR_PAGE_COMPOSE_H

#include <stddef.h>
#include <stdint.h>

#include "image.h"     /* OcrImage */
#include "wubufont.h"  /* Font */

/* Layout arrangement for a composed page. Most real text is NOT a random
 * scatter -- it is organized as lines, paragraphs, or (for many Asian
 * layouts) a line-grid. The composer can emit any of these so the OCR
 * pipeline is exercised on structured pages, not just a crowd. */
typedef enum {
    OCR_LAYOUT_SCATTER,   /* random placement, per-glyph 2D+3D warp (gauntlet) */
    OCR_LAYOUT_LINES,     /* left-to-right lines, per-glyph warp, baseline grid */
    OCR_LAYOUT_GRID       /* line-grid: rows x cols cells (CJK/Asian books) */
} OcrComposeLayout;

/* Compose a page of glyphs drawn from `fonts` (nfonts), each a random char
 * from `chars` (nchars). Placement/warping depends on `layout`:
 *   SCATTER: random position with a random blend of 2D rotation + 3D
 *            perspective/shear (the user's "random around the page" gauntlet).
 *   LINES:   glyphs laid out left-to-right in `rows` baseline-aligned lines,
 *            each glyph still individually warped (line/paragraph model).
 *   GRID:    glyphs placed in a rows x cols cell grid (line-grid model).
 * Returns a malloc'd OcrImage (white bg, black ink) or NULL on OOM/empty
 * input. The caller owns it.
 *
 *   W,H   : page size in pixels.
 *   ppm   : raster resolution for source glyphs.
 *   seed  : PRNG seed (deterministic output for a given seed).
 *   maxrot: max |2D rotation| in degrees (per glyph).
 *   maxpersp: max |3D perspective pinch| (0..0.6 reasonable).
 *   maxshear: max |3D shear| in degrees (per glyph).
 *   rows,cols: line count (LINES) or grid dimensions (GRID). Ignored for
 *              SCATTER (which sizes its own crowd).
 *   out_placed: if non-NULL, set to the number of glyphs actually placed. */
OcrImage *ocr_compose_page_ex(const Font *const *fonts, size_t nfonts,
                              const char *const *chars, size_t nchars,
                              size_t W, size_t H, int ppm, unsigned seed,
                              double maxrot, double maxpersp, double maxshear,
                              OcrComposeLayout layout, size_t rows, size_t cols,
                              size_t *out_placed);

/* Backwards-compatible scatter composer (delegates to ocr_compose_page_ex). */
OcrImage *ocr_compose_page(const Font *const *fonts, size_t nfonts,
                           const char *const *chars, size_t nchars,
                           size_t W, size_t H, int ppm, unsigned seed,
                           double maxrot, double maxpersp, double maxshear,
                           size_t *out_placed);

/* Deterministic single-line control page: `text` rendered left-to-right from
 * `font` with no warping. Returns a malloc'd OcrImage or NULL. */
OcrImage *ocr_compose_line(const Font *font, const char *text, int ppm);

#endif /* WUBUOCR_PAGE_COMPOSE_H */
