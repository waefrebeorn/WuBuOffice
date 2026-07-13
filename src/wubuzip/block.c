#include "block.h"
#include "bit.h"
#include "huffman.h"

#include <stdlib.h>
#include <string.h>

const uint16_t wubuzip_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
const uint8_t wubuzip_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
const uint16_t wubuzip_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
    1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
const uint8_t wubuzip_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

int wubuzip_buf_put(wubuzip_buf *b, uint8_t c) {
    if (b->len == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4096;
        uint8_t *n = realloc(b->p, b->cap);
        if (!n) return -1;
        b->p = n;
    }
    b->p[b->len++] = c;
    return 0;
}

int wubuzip_block_decode(void *br, const void *lh, const void *dh, void *out) {
    wubuzip_bitreader *b = (wubuzip_bitreader *)br;
    wubuzip_buf *ob = (wubuzip_buf *)out;
    const wubuzip_huff *lit = (const wubuzip_huff *)lh;
    const wubuzip_huff *dist = (const wubuzip_huff *)dh;

    for (;;) {
        int sym = wubuzip_huff_decode(b, lit);
        if (sym < 0) return -1;
        if (sym == 256) return 0;                 /* end of block */
        if (sym < 256) {
            if (wubuzip_buf_put(ob, (uint8_t)sym)) return -1;
            continue;
        }
        sym -= 257;
        if (sym >= 29) return -1;
        int length = wubuzip_len_base[sym] + (int)wubuzip_bit_get(b, wubuzip_len_extra[sym]);
        int ds = wubuzip_huff_decode(b, dist);
        if (ds < 0 || ds >= 30) return -1;
        int distance = wubuzip_dist_base[ds] + (int)wubuzip_bit_get(b, wubuzip_dist_extra[ds]);
        if ((size_t)distance > ob->len) return -1;
        for (int i = 0; i < length; i++) {
            uint8_t c = ob->p[ob->len - (size_t)distance];
            if (wubuzip_buf_put(ob, c)) return -1;
        }
    }
}

int wubuzip_block_dynamic(void *br, void *lh, void *dh, void *out) {
    wubuzip_bitreader *b = (wubuzip_bitreader *)br;
    wubuzip_huff *lit = (wubuzip_huff *)lh;
    wubuzip_huff *dist = (wubuzip_huff *)dh;

    int hlit = (int)wubuzip_bit_get(b, 5) + 257;
    int hdist = (int)wubuzip_bit_get(b, 5) + 1;
    int hclen = (int)wubuzip_bit_get(b, 4) + 4;
    static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

    uint8_t cl_len[19];
    memset(cl_len, 0, sizeof cl_len);
    for (int i = 0; i < hclen; i++) cl_len[ORD[i]] = (uint8_t)wubuzip_bit_get(b, 3);

    wubuzip_huff clh;
    if (wubuzip_huff_build(&clh, cl_len, 19) != 0) return -1;

    uint8_t lengths[288 + 32];
    int n = hlit + hdist;
    int i = 0;
    while (i < n) {
        int s = wubuzip_huff_decode(b, &clh);
        if (s < 0) return -1;
        if (s < 16) {
            lengths[i++] = (uint8_t)s;
        } else if (s == 16) {
            if (i == 0) return -1;
            int rep = (int)wubuzip_bit_get(b, 2) + 3;
            uint8_t v = lengths[i - 1];
            while (rep-- > 0 && i < n) lengths[i++] = v;
        } else if (s == 17) {
            int rep = (int)wubuzip_bit_get(b, 3) + 3;
            while (rep-- > 0 && i < n) lengths[i++] = 0;
        } else if (s == 18) {
            int rep = (int)wubuzip_bit_get(b, 7) + 11;
            while (rep-- > 0 && i < n) lengths[i++] = 0;
        } else {
            return -1;
        }
    }
    if (wubuzip_huff_build(lit, lengths, hlit) != 0) return -1;
    if (wubuzip_huff_build(dist, lengths + hlit, hdist) != 0) return -1;
    return wubuzip_block_decode(b, lit, dist, out);
}
