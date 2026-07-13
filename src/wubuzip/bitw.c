#include "bitw.h"

#include <stdlib.h>
#include <string.h>

void wubuzip_bitw_init(wubuzip_bitwriter *w) {
    w->p = NULL;
    w->len = 0;
    w->cap = 0;
    w->bitbuf = 0;
    w->bitcnt = 0;
}

void wubuzip_bitw_free(wubuzip_bitwriter *w) {
    free(w->p);
    w->p = NULL;
    w->len = w->cap = 0;
}

int wubuzip_bitw_put(wubuzip_bitwriter *w, uint32_t v, int n) {
    /* Use a 64-bit accumulator so code bits (up to 25) merging with up to 7
     * buffered bits never overflow the low word. */
    w->bitbuf |= ((uint64_t)(v & ((UINT32_C(1) << n) - 1u)) << w->bitcnt);
    w->bitcnt += n;
    while (w->bitcnt >= 8) {
        if (w->len == w->cap) {
            size_t nc = w->cap ? w->cap * 2 : 256;
            uint8_t *np = realloc(w->p, nc);
            if (!np) return -1;
            w->p = np;
            w->cap = nc;
        }
        w->p[w->len++] = (uint8_t)(w->bitbuf & 0xFF);
        w->bitbuf >>= 8;
        w->bitcnt -= 8;
    }
    return 0;
}

int wubuzip_bitw_byte(wubuzip_bitwriter *w, uint8_t b) {
    /* byte must be aligned for stored-block framing; flush bits first. */
    if (w->bitcnt != 0) {
        if (w->bitcnt >= 8) return -1;
        if (wubuzip_bitw_align(w) != 0) return -1;
    }
    if (w->len == w->cap) {
        size_t nc = w->cap ? w->cap * 2 : 256;
        uint8_t *np = realloc(w->p, nc);
        if (!np) return -1;
        w->p = np;
        w->cap = nc;
    }
    w->p[w->len++] = b;
    return 0;
}

int wubuzip_bitw_align(wubuzip_bitwriter *w) {
    if (w->bitcnt > 0) {
        if (w->bitcnt > 7) return -1; /* would lose bits */
        if (w->len == w->cap) {
            size_t nc = w->cap ? w->cap * 2 : 256;
            uint8_t *np = realloc(w->p, nc);
            if (!np) return -1;
            w->p = np;
            w->cap = nc;
        }
        w->p[w->len++] = (uint8_t)(w->bitbuf & 0xFF);
        w->bitbuf = 0;
        w->bitcnt = 0;
    }
    return 0;
}

int wubuzip_bitw_finish(wubuzip_bitwriter *w, uint8_t **out, size_t *out_len) {
    if (wubuzip_bitw_align(w) != 0) return -1;
    uint8_t *buf = realloc(w->p, w->len + 1);
    if (!buf) return -1;
    buf[w->len] = 0;
    *out = buf;
    *out_len = w->len;
    w->p = NULL; /* ownership transferred */
    return 0;
}
