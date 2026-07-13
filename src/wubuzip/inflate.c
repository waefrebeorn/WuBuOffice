#include "inflate.h"
#include "bit.h"
#include "huffman.h"
#include "fixed.h"
#include "block.h"

#include <stdlib.h>

int wubuzip_inflate(const uint8_t *in, size_t in_len,
                    uint8_t **out, size_t *out_len, int with_zlib_header) {
    wubuzip_bitreader br;
    wubuzip_bit_init(&br, in, in_len);

    if (with_zlib_header) {
        (void)wubuzip_bit_get(&br, 8); /* CMF */
        (void)wubuzip_bit_get(&br, 8); /* FLG */
    }

    wubuzip_buf ob = {0};
    wubuzip_huff lit, dist;
    int rc = 0;
    size_t guard = 0;

    for (;;) {
        if (++guard > 1000000) { rc = -1; break; }  /* corrupt-stream guard */
        int bfinal = (int)wubuzip_bit_get(&br, 1);
        int btype = (int)wubuzip_bit_get(&br, 2);

        if (btype == 0) {
            /* stored (uncompressed) block */
            wubuzip_bit_align(&br);
            if (br.bytepos + 4 > br.len) { rc = -1; break; }
            uint32_t len = (uint32_t)in[br.bytepos]
                         | ((uint32_t)in[br.bytepos + 1] << 8);
            size_t start = br.bytepos + 4;           /* skip LEN + NLEN */
            if (start + len > br.len) { rc = -1; break; }
            for (uint32_t i = 0; i < len; i++)
                if (wubuzip_buf_put(&ob, in[start + i])) { rc = -1; break; }
            if (rc != 0) break;
            br.bytepos = start + len;
        } else if (btype == 1) {
            uint8_t ll[288], d[32];
            wubuzip_fixed_litlen(ll);
            wubuzip_fixed_dist(d);
            if (wubuzip_huff_build(&lit, ll, 288) != 0 ||
                wubuzip_huff_build(&dist, d, 32) != 0) { rc = -1; break; }
            rc = wubuzip_block_decode(&br, &lit, &dist, &ob);
            if (rc != 0) break;
        } else if (btype == 2) {
            rc = wubuzip_block_dynamic(&br, &lit, &dist, &ob);
            if (rc != 0) break;
        } else {
            rc = -1; break;
        }

        if (bfinal) break;
    }

    if (rc != 0) {
        free(ob.p);
        return -1;
    }
    *out = ob.p;
    *out_len = ob.len;
    return 0;
}
