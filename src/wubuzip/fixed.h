#ifndef WUBUZIP_FIXED_H
#define WUBUZIP_FIXED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RFC 1951 section 3.2.6 fixed Huffman tables. The literal/length code
 * lengths are: 0-143 -> 8 bits, 144-255 -> 9 bits, 256-279 -> 7 bits,
 * 280-287 -> 8 bits. Distance codes are all 5 bits (32 codes).
 *
 * These are produced by wubuzip_fixed_init() at runtime into the buffers you
 * pass in. Generating them avoids a hand-written 288-entry literal that can be
 * truncated or mis-edited (the exact failure that broke an earlier build). */

void wubuzip_fixed_litlen(uint8_t out[288]);
void wubuzip_fixed_dist(uint8_t out[32]);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_FIXED_H */
