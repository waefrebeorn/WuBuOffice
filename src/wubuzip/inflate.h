#ifndef WUBUZIP_INFLATE_H
#define WUBUZIP_INFLATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* From-scratch RFC 1951 DEFLATE inflate. Supports store (method 0) and
 * deflate (method 8) with fixed and dynamic Huffman blocks. Enough to read
 * real OOXML parts (Word/LibreOffice compress document.xml etc. with deflate). */

/* Inflate `in[0..in_len)` (raw DEFLATE stream, i.e. with the zlib 2-byte header
 * already stripped, or with zlib header if with_zlib_header=1) into `out`,
 * growing as needed. On success returns 0 and sets *out_len; on error returns
 * -1. The output buffer is heap-allocated and owned by the caller (free it). */
int wubuzip_inflate(const uint8_t *in, size_t in_len,
                    uint8_t **out, size_t *out_len, int with_zlib_header);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_INFLATE_H */
