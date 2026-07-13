#ifndef WUBUZIP_FIXEDCODE_H
#define WUBUZIP_FIXEDCODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical fixed-Huffman *encode* tables (RFC 1951 3.2.6).
 *
 * Literal/length alphabet (288 symbols): codes 0..287.
 * Distance alphabet (32 symbols): all 5-bit codes.
 *
 * wubuzip_fixed_code_init fills `code` (the canonical bit pattern) and `len`
 * (the code length in bits) for each symbol. Code lengths match the decoder's
 * wubuzip_fixed_litlen()/fixed_dist(). */
void wubuzip_fixed_code_init(uint16_t lit_code[288], uint8_t lit_len[288],
                             uint16_t dist_code[32], uint8_t dist_len[32]);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_FIXEDCODE_H */
