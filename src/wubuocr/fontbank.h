/* fontbank.h -- multi-font template bank for WuBuOCR.
 *
 * Implements the user's "study many font types" idea: instead of a single
 * baked-in 8x8 bitmap font, we rasterize REAL fonts (via the clean-room
 * wubufont rasterizer) into zoning templates and keep a *bank* of several
 * of them. A candidate glyph is classified independently against every font
 * in the bank; the answer is the glyph that wins the most per-font votes
 * (winner-takes-most). This is far more robust than one font: a letter
 * that is ambiguous in DejaVu may be obvious in Liberation, and vice
 * versa, and the vote reconciles them.
 *
 * The bank exposes the exact OcrRecognizer signature used by the rest of the
 * OCR pipeline (wubuocr.h), so it drops into ocr_page_analyze() with no
 * change to the call site:
 *
 *     OcrFontBank *bank = ocr_fontbank_build(fonts, nfonts, GRID, PPM);
 *     ocr_page_analyze(im, NULL, ocr_fontbank_recognizer(), bank);
 *
 * Opaque bank handle (soul.md sec.10): callers never touch the per-font
 * template arrays. Self-contained: depends only on wubufont (rasterizer)
 * and the public OCR pipeline types.
 */
#ifndef WUBUOCR_FONTBANK_H
#define WUBUOCR_FONTBANK_H

#include <stddef.h>
#include <stdint.h>

#include "wubuocr.h"   /* OcrRecognizer, OcrBinary, OcrBlock, OcrImage */

typedef struct OcrFontBank OcrFontBank;

/* Build a multi-font bank.
 *   fonts   : array of opaque Font* (from wubufont font_open); the bank
 *             does NOT take ownership -- caller keeps them alive.
 *   nfonts  : number of fonts in `fonts` (0..OCR_FONTBANK_MAX).
 *   grid    : zoning grid size N (typical 4..6). Same meaning as the
 *             single-font recognizer's grid.
 *   ppm     : rasterization resolution (pixels per em) used to render each
 *             reference glyph from the real fonts.
 *   classes : NULL-terminated array of glyph strings (e.g. ASCII chars or
 *             the English+Latin composite from latin1.h). Pass NULL for the
 *             default English tier (printable ASCII 0x20..0x7E).
 * Returns NULL on OOM or if no font could contribute templates. */
OcrFontBank *ocr_fontbank_build(const void *const *fonts, size_t nfonts,
                                size_t grid, int ppm, const char *const *classes);

/* Convenience: build a bank over the English (printable ASCII) class set. */
OcrFontBank *ocr_fontbank_build_english(const void *const *fonts, size_t nfonts,
                                        size_t grid, int ppm);

/* Free a bank (does not free the underlying Font* objects). */
void ocr_fontbank_free(OcrFontBank *bank);

/* Number of fonts that successfully contributed to the bank (<= nfonts). */
size_t ocr_fontbank_font_count(const OcrFontBank *bank);

/* OcrRecognizer bound to a bank: pass the OcrFontBank* as the `user`
 * pointer to ocr_page_analyze(). Returns a malloc'd 1-char UTF-8 string
 * for the best-voted glyph, or NULL if the bank rejects the candidate
 * (below the per-font confidence gate across all fonts, or a tie). */
char *ocr_fontbank_recognize(const OcrBinary *b, const OcrBlock *glyph,
                             void *user);

/* Convenience accessor so callers wire the pair in one place. */
OcrRecognizer ocr_fontbank_recognizer(void);

/* Maximum number of fonts a bank can hold (compile-time cap). */
#define OCR_FONTBANK_MAX 16

#endif /* WUBUOCR_FONTBANK_H */
