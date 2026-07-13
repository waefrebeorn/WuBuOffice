#ifndef WUBUZIP_LZ77_H
#define WUBUZIP_LZ77_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Greedy + lazy LZ77 matcher for DEFLATE. Finds (length, distance) tokens
 * over the input. Uses a 3-byte hash => chain of candidate positions (like
 * zlib's lazy matching) for O(n) typical performance instead of O(n*WIN). */

#define WUBUZIP_LZ_HASH_BITS 15
#define WUBUZIP_LZ_HASH_SIZE (1 << WUBUZIP_LZ_HASH_BITS)
#define WUBUZIP_LZ_WIN 32768
#define WUBUZIP_LZ_NICE 258     /* stop extending past this match length */
#define WUBUZIP_LZ_CHAIN 256    /* max candidates scanned per position */

typedef struct wubuzip_lz wubuzip_lz;

/* A token in the parsed stream. kind:
 *   0 -> literal byte `lit`
 *   1 -> match of `len` (3..258) at backward `dist` (1..32768) */
typedef struct {
    uint8_t kind;     /* 0 literal, 1 match */
    uint8_t lit;
    uint16_t len;
    uint16_t dist;
} wubuzip_lz_tok;

/* Parse `in[0..in_len)` into tokens. Caller frees with wubuzip_lz_free().
 * Returns 0 on success, -1 on allocation failure. `min_match` is normally 3. */
int wubuzip_lz_parse(const uint8_t *in, size_t in_len, wubuzip_lz **out);

/* Accessors. */
const wubuzip_lz_tok *wubuzip_lz_tokens(const wubuzip_lz *lz);
size_t wubuzip_lz_count(const wubuzip_lz *lz);

void wubuzip_lz_free(wubuzip_lz *lz);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_LZ77_H */
