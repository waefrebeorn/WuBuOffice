/* img2doc.h -- image -> OCR'd document bridge.
 *
 * Turns a decoded RGBA raster (from the clean-room wubuimage PNG decoder, or
 * any source) into the wubuocr pipeline: flatten to 8-bit grayscale, run the
 * recognizer, and emit wubudoc "document" model JSON (paragraph-per-block).
 *
 * Self-contained: depends only on wubuocr (the pipeline) + wubujson. The
 * recognizer is injected by the caller (font-bank template engine) so this
 * module stays free of font loading -- it is pure raster->document plumbing.
 */
#ifndef WUBUIMAGE_IMG2DOC_H
#define WUBUIMAGE_IMG2DOC_H

#include <stddef.h>
#include <stdint.h>

#include "wubuocr.h"   /* OcrRecognizer, OcrImage, OcrPage */
#include "fontbank.h"  /* OcrFontBank */

#ifdef __cplusplus
extern "C" {
#endif

/* Recognize text from an RGBA raster (w x h, row-major w*h*4 bytes) using the
 * supplied recognizer + its `user` context (typically an OcrFontBank*).
 * Returns malloc'd wubudoc "document" model JSON (caller frees), or NULL on
 * failure / empty page. */
char *img2doc_recognize_rgba(const uint8_t *rgba, size_t w, size_t h,
                             OcrRecognizer rec, void *user);

/* Build a default multi-font recognizer bank from the system's real fonts
 * (DejaVu / Liberation / FreeSans, whichever exist). The returned bank (and the
 * fonts it references) must outlive any recognition call; free with
 * ocr_fontbank_free() when done. Returns NULL if no usable font exists. */
OcrFontBank *img2doc_default_bank(void);

/* Convenience: decode a PNG blob and OCR it with the given bank. Returns
 * malloc'd wubudoc "document" model JSON (caller frees), or NULL. */
char *img2doc_recognize_png(const uint8_t *png, size_t len, OcrFontBank *bank);

#ifdef __cplusplus
}
#endif

#endif /* WUBUIMAGE_IMG2DOC_H */
