/* png.h -- clean-room PNG decoder (dependency-free, RFC 2083 subset).
 *
 * Decodes the PNG image format into a simple RGBA pixel plane. No libpng:
 * the zlib/DEFLATE stream is inflated with the in-tree wubuzip engine, and
 * the per-scanline filters (None/Sub/Up/Average/Paeth) are applied here.
 *
 * Supported: 8-bit depth, colour types 0 (gray), 2 (RGB), 3 (palette),
 * 4 (gray+alpha), 6 (RGBA). Interlace (Adam7) is NOT supported -- single
 * image data stream only, which is what every real-world scan/export emits.
 *
 * Opaque handle (soul.md sec.10): callers read pixels only through the API.
 */
#ifndef WUBUIMAGE_PNG_H
#define WUBUIMAGE_PNG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PngImage PngImage;

/* Decode a PNG blob. Returns NULL on malformed input or unsupported format.
 * The input need not outlive the image. */
PngImage *png_decode(const uint8_t *data, size_t len);

void png_free(PngImage *im);

size_t png_width(const PngImage *im);
size_t png_height(const PngImage *im);

/* Row-major RGBA plane, w*h*4 bytes. 0=transparent .. 255=opaque. */
const uint8_t *png_rgba(const PngImage *im);

#ifdef __cplusplus
}
#endif

#endif /* WUBUIMAGE_PNG_H */
