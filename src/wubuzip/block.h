#ifndef WUBUZIP_BLOCK_H
#define WUBUZIP_BLOCK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Length/distance base tables (RFC 1951 3.2.5). */
#define WUBUZIP_LEN_BASE_N 29
extern const uint16_t wubuzip_len_base[29];
extern const uint8_t  wubuzip_len_extra[29];
extern const uint16_t wubuzip_dist_base[30];
extern const uint8_t  wubuzip_dist_extra[30];

/* Map a match length (3..258) to its literal/length code index (0..28). */
int wubuzip_len_sym(int len);

/* Map a match distance (1..32768) to its distance code index (0..29). */
int wubuzip_dist_sym(int dist);

/* Decode a single block into `out` (growable buffer). `br` is the bit reader,
 * `lh`/`dh` the literal/length and distance Huffman tables. Returns 0 on
 * end-of-block, -1 on error. */
int wubuzip_block_decode(void *br, const void *lh, const void *dh, void *out);

/* Build the literal/length + distance tables for a dynamic block and decode
 * it. Returns 0 on end-of-block, -1 on error. */
int wubuzip_block_dynamic(void *br, void *lh, void *dh, void *out);

/* Growable byte sink used by the decoders. */
typedef struct wubuzip_buf {
    uint8_t *p;
    size_t len;
    size_t cap;
} wubuzip_buf;

int wubuzip_buf_put(wubuzip_buf *b, uint8_t c);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_BLOCK_H */
