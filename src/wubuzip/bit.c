#include "bit.h"

void wubuzip_bit_init(wubuzip_bitreader *b, const uint8_t *data, size_t len) {
    b->data = data;
    b->len = len;
    b->bytepos = 0;
    b->bitbuf = 0;
    b->bitcnt = 0;
}

uint32_t wubuzip_bit_get(wubuzip_bitreader *b, int n) {
    while (b->bitcnt < n) {
        if (b->bytepos >= b->len) {
            /* feed zero bits when we run off the end of the buffer */
            b->bitbuf |= (uint32_t)0;
            b->bitcnt += 8;
        } else {
            b->bitbuf |= (uint32_t)(b->data[b->bytepos++]) << b->bitcnt;
            b->bitcnt += 8;
        }
    }
    uint32_t v = b->bitbuf & ((UINT32_C(1) << n) - 1u);
    b->bitbuf >>= n;
    b->bitcnt -= n;
    return v;
}

void wubuzip_bit_align(wubuzip_bitreader *b) {
    b->bitbuf = 0;
    b->bitcnt = 0;
}
