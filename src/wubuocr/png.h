/* png.h -- clean-room PNG decoder -> OcrImage grayscale.
 * Dependency-free: uses wubuzip_inflate (from-scratch DEFLATE) + a local CRC32.
 * Supports the common cases a phone photo / scanned doc produces:
 *   8-bit grayscale, 24-bit RGB, 32-bit RGBA (alpha blended over white).
 * Interlace (Adam7) is NOT supported (reject with NULL + a flag) — fine for
 * documents, which are never interlaced. 16-bit is down-shifted to 8-bit.
 */
#ifndef WUBUOCR_PNG_H
#define WUBUOCR_PNG_H
#include "image.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Decode PNG bytes -> OcrImage (grayscale 8bpp). Returns NULL on failure.
 * If non-NULL, *was_interlaced (if provided) reports whether the image was
 * Adam7 (we reject those -> NULL). */
OcrImage *ocr_image_from_png(const uint8_t *data, size_t len, int *was_interlaced);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOCR_PNG_H */
