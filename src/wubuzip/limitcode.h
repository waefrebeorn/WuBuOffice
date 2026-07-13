#ifndef WUBUZIP_LIMITCODE_H
#define WUBUZIP_LIMITCODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Length-limited Huffman code-length generator (package-merge, Larmore &
 * Hirschberg). Given symbol weights and a max code length, produces code
 * LENGTHS that minimize total weighted length subject to the limit. This is
 * what DEFLATE needs for dynamic blocks: the combined code-length alphabet
 * (19 symbols) must itself be length-limited to 7 bits, and the literal and
 * distance alphabets to 15 bits.
 *
 * `freqs`   weights (e.g. symbol occurrence counts), length n.
 * `n`       alphabet size.
 * `maxlen`  maximum allowed code length (e.g. 7 for the codelen code, 15 for
 *           literal/length and distance).
 * `len`     output: code length for each symbol (0 = unused).
 *
 * Returns 0 on success, -1 on allocation failure. */
int wubuzip_limit_lengths(const uint32_t *freqs, int n, int maxlen, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_LIMITCODE_H */
