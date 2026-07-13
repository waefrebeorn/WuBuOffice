#ifndef WUBUZIP_HUFFMAN_H
#define WUBUZIP_HUFFMAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical Huffman decoder table built from code lengths. Self-contained:
 * no dependencies. `sym` holds up to 288 symbols (the maximum literal/length
 * alphabet); distance alphabets are 30 max but we size generously. */
#define WUBUZIP_HUFF_MAX_SYM 288

typedef struct wubuzip_huff {
    uint16_t cnt[16];   /* number of codes of each length 1..15 */
    uint16_t sym[WUBUZIP_HUFF_MAX_SYM];
} wubuzip_huff;

/* Build a decode table from an array of code lengths (0 = unused). `n` is the
 * alphabet size (e.g. 288 for literal/length, 30 for distance, 19 for the
 * code-length code, or 32 for the fixed distance code). Returns 0, or -1 if
 * the lengths describe an invalid prefix code. */
int wubuzip_huff_build(wubuzip_huff *h, const uint8_t *lengths, int n);

/* Decode one symbol using the bit reader. Returns the symbol, or -1 on error.
 * `br` is a wubuzip_bitreader* passed opaquely via the forward-typed pointer. */
int wubuzip_huff_decode(void *br, const wubuzip_huff *h);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_HUFFMAN_H */
