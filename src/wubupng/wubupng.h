#ifndef WUBUPNG_H
#define WUBUPNG_H
/* wubupng -- single, correct, dependency-free PNG encoder for WuBuOffice.
 *
 * Replaces the two divergent encoders we had (apps/wubuwordview/main.c RGBA,
 * src/wubuocr/png_encode.c grayscale) so there is ONE chunk-framing/CRC core
 * -- that divergence is exactly how a PNG bug gets silently copied. Supports
 * RGBA (32-bit) and GRAY8 (8-bit) output, zlib-deflated (valid for any
 * compliant decoder). Uses zlib (a system lib, not bundled third-party).
 *
 * C11, 0 warnings under -Wall -Wextra -Wpedantic.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WUBUPNG_RGBA   32   /* 4 bytes/pixel: R,G,B,A (A ignored, forced 255) */
#define WUBUPNG_GRAY8   8   /* 1 byte/pixel:  grayscale                         */

/* Encode pixels into a malloc'd PNG byte buffer.
 * fmt: WUBUPNG_RGBA or WUBUPNG_GRAY8. pixels: W*H px, row-major, no filter byte.
 * Returns 0 on success (out and out_len are set; caller frees), -1 on error. */
int wubupng_encode(int fmt, const void *pixels, uint32_t W, uint32_t H,
                   uint8_t **out, size_t *out_len);

/* Convenience: encode and write directly to a file. Returns 0 ok. */
int wubupng_write_file(const char *path, int fmt, const void *pixels,
                       uint32_t W, uint32_t H);

#ifdef __cplusplus
}
#endif
#endif /* WUBUPNG_H */
