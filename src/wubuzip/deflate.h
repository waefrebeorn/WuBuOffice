#ifndef WUBUZIP_DEFLATE_H
#define WUBUZIP_DEFLATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Raw DEFLATE encoder (RFC 1951), fixed-Huffman blocks. Produces a stream with
 * NO zlib wrapper, suitable for embedding directly inside a ZIP "method 8"
 * member or a .gz payload. The companion wubuzip_inflate() decodes it.
 *
 * Returns 0 on success and transfers ownership of *out (caller frees with
 * free()). Returns -1 on allocation failure. */
int wubuzip_deflate(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_DEFLATE_H */
