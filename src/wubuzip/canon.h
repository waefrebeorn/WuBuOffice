#ifndef WUBUZIP_CANON_H
#define WUBUZIP_CANON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute canonical Huffman codes + lengths for a given alphabet.
 *
 * `freqs`  frequency (or any positive weight) per symbol, length n.
 * `n`      alphabet size (e.g. 288 literal/length, 32 distance, 19 codelen).
 * `maxlen` maximum code length allowed (15 for DEFLATE, or a tighter limit
 *          when building a length-limited code for the dynamic-block header).
 *
 * On success returns 0 and fills:
 *   `code[i]` canonical code bit pattern for symbol i (undefined if len==0)
 *   `len[i]`  code length in bits for symbol i (0 = symbol unused)
 *
 * The caller is responsible for ensuring the frequencies describe a code that
 * fits within `maxlen` symbols; if not, returns -1. For a single-symbol or
 * empty alphabet it synthesizes a 1-bit code so the symbol remains decodable. */
int wubuzip_canon(const uint32_t *freqs, int n, int maxlen,
                  uint16_t *code, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_CANON_H */
