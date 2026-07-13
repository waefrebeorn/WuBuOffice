#include "huffman.h"
#include "bit.h"

#include <string.h>

/* Build a canonical Huffman decode table from code lengths (RFC 1951 3.2.2).
 * The canonical assignment: codes of length L are consecutive, and shorter
 * lengths always sort numerically smaller. We capture that with per-length
 * counts plus an offset table, exactly the construction used by puff.c. */
int wubuzip_huff_build(wubuzip_huff *h, const uint8_t *lengths, int n) {
    int cnt[16];
    int offs[16];
    int left;

    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) cnt[lengths[i]]++;
    cnt[0] = 0;

    /* left = 2^len - sum(count[len]); must end at 0 (complete code). */
    left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= cnt[len];
        if (left < 0) return -1;        /* over-subscribed */
    }
    if (left != 0 && (n == 0 || left != 1)) return -1; /* incomplete (unless empty) */

    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = offs[len] + cnt[len];

    /* assign symbol indices in ascending symbol order */
    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len != 0) h->sym[offs[len]++] = (uint16_t)i;
    }
    for (int i = 1; i < 16; i++) h->cnt[i] = (uint16_t)cnt[i];
    h->cnt[0] = 0;
    return 0;
}

/* Canonical decode (Adler, puff.c). `br` is a wubuzip_bitreader*. */
int wubuzip_huff_decode(void *br, const wubuzip_huff *h) {
    wubuzip_bitreader *b = (wubuzip_bitreader *)br;
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        code |= (int)wubuzip_bit_get(b, 1);
        int count = h->cnt[len];
        if (code - first < count)
            return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}
