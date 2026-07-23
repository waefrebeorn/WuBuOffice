/* png_encode.h -- minimal PNG encoder (grayscale 8-bit) for test dumps/debug.
 * Clean-room: zlib-style deflate wrapper around wubuzip_inflate is decode-only,
 * so we STORE (method 0) the scanlines (no compression). Fine for debug dumps.
 */
#ifndef WUBUOCR_PNG_ENCODE_H
#define WUBUOCR_PNG_ENCODE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Encode a WxH 8-bit grayscale buffer (row-major, 0=black..255=white) into a
 * PNG blob. *out is heap-allocated (caller frees). Returns 0 on success. */
int png_encode_gray(const uint8_t *gray, uint32_t W, uint32_t H,
                    uint8_t **out, size_t *out_len);
#ifdef __cplusplus
}
#endif
#endif
