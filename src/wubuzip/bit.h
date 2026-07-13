#ifndef WUBUZIP_BIT_H
#define WUBUZIP_BIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LSB-first bit reader over an in-memory byte buffer. Self-contained: no
 * dependencies beyond the C standard library. */
typedef struct wubuzip_bitreader {
    const uint8_t *data;
    size_t len;
    size_t bytepos;
    uint32_t bitbuf;
    int bitcnt;
} wubuzip_bitreader;

void wubuzip_bit_init(wubuzip_bitreader *b, const uint8_t *data, size_t len);

/* Read the low `n` (1..25) bits LSB-first. Past end-of-input it feeds zero
 * bits (a corrupt stream is the caller's responsibility to bound). */
uint32_t wubuzip_bit_get(wubuzip_bitreader *b, int n);

/* Discard the current partial byte and realign to the next byte boundary. */
void wubuzip_bit_align(wubuzip_bitreader *b);

#ifdef __cplusplus
}
#endif

#endif /* WUBUZIP_BIT_H */
