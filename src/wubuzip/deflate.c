#include "deflate.h"
#include "bitw.h"
#include "fixedcode.h"
#include "block.h"

#include <stdlib.h>
#include <string.h>

/* Max match we will emit: RFC 1951 length codes top out at 258. */
#define MAX_MATCH 258
/* How far back we look for a match (distance <= 32768). We use a simple
 * brute-force window scan capped to keep it O(n*WIN) worst case small. */
#define WIN 32768

/* DEFLATE packs Huffman codes with the MOST-significant code bit first, even
 * though the rest of the bitstream (BFINAL/BTYPE/extra bits) is LSB-first.
 * Emit `v`'s low `n` bits MSB-first into the LSB-first byte stream. */
static int bitw_put_code(wubuzip_bitwriter *w, uint32_t v, int n) {
    for (int k = n - 1; k >= 0; k--)
        if (wubuzip_bitw_put(w, (v >> k) & 1u, 1) != 0) return -1;
    return 0;
}

/* Emit a literal symbol via the fixed literal/length codes. */
static int emit_lit(wubuzip_bitwriter *w, uint16_t code, uint8_t len) {
    return bitw_put_code(w, code, len);
}

/* Emit a <length, distance> pair. `len` is the actual match length (3..258);
 * `dist` is the actual backward distance (1..32768). */
static int emit_match(wubuzip_bitwriter *w,
                      const uint16_t *lit_code, const uint8_t *lit_len,
                      const uint16_t *dist_code, const uint8_t *dist_len,
                      int len, int dist) {
    /* length symbol 257..285 */
    int lsym = 0;
    for (int i = 0; i < 29; i++) {
        int hi = wubuzip_len_base[i] + (1 << wubuzip_len_extra[i]) - 1;
        if (len >= wubuzip_len_base[i] && len <= hi) { lsym = 257 + i; break; }
    }
    if (bitw_put_code(w, lit_code[lsym], lit_len[lsym]) != 0) return -1;
    int li = lsym - 257;
    if (wubuzip_len_extra[li])
        if (wubuzip_bitw_put(w, (uint32_t)(len - wubuzip_len_base[li]), wubuzip_len_extra[li]) != 0) return -1;

    /* distance symbol 0..29 */
    int dsym = 0;
    for (int i = 0; i < 30; i++) {
        int hi = wubuzip_dist_base[i] + (1 << wubuzip_dist_extra[i]) - 1;
        if (dist >= wubuzip_dist_base[i] && dist <= hi) { dsym = i; break; }
    }
    if (bitw_put_code(w, dist_code[dsym], dist_len[dsym]) != 0) return -1;
    if (wubuzip_dist_extra[dsym])
        if (wubuzip_bitw_put(w, (uint32_t)(dist - wubuzip_dist_base[dsym]), wubuzip_dist_extra[dsym]) != 0) return -1;
    return 0;
}

static int longest_match(const uint8_t *in, size_t pos, size_t in_len,
                         int *dist_out) {
    int best = 0;
    int best_dist = 0;
    size_t start = pos > WIN ? pos - WIN : 0;
    /* Scan strictly BEHIND the current position: a match must copy from
     * already-emitted bytes, so distance >= 1. Starting at j == pos would
     * compare in[pos+run] with itself (always equal) and fabricate a
     * distance-0 match, which is illegal in DEFLATE. */
    for (size_t j = pos; j-- > start; ) {
        if (in[j] != in[pos]) continue;
        int run = 0;
        while ((size_t)(pos + run) < in_len &&
               run < MAX_MATCH &&
               in[j + run] == in[pos + run])
            run++;
        if (run > best) {
            best = run;
            best_dist = (int)(pos - j);
            if (best >= MAX_MATCH) break;
        }
    }
    *dist_out = best_dist;
    return best;
}

int wubuzip_deflate(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
    uint16_t lit_code[288]; uint8_t lit_len[288];
    uint16_t dist_code[32]; uint8_t dist_len[32];
    wubuzip_fixed_code_init(lit_code, lit_len, dist_code, dist_len);

    wubuzip_bitwriter w;
    wubuzip_bitw_init(&w);

    /* Single DEFLATE stream: one final fixed-Huffman block. */
    if (wubuzip_bitw_put(&w, 1, 1) != 0) goto fail;   /* BFINAL = 1 */
    if (wubuzip_bitw_put(&w, 1, 2) != 0) goto fail;   /* BTYPE = 01 (fixed) */

    size_t i = 0;
    while (i < in_len) {
        int dist = 0;
        int ml = (in_len - i >= 3) ? longest_match(in, i, in_len, &dist) : 0;
        if (ml >= 3) {
            if (emit_match(&w, lit_code, lit_len, dist_code, dist_len, ml, dist) != 0) goto fail;
            i += (size_t)ml;
        } else {
            if (emit_lit(&w, lit_code[in[i]], lit_len[in[i]]) != 0) goto fail;
            i += 1;
        }
    }

    /* end-of-block symbol 256 (codes are MSB-first) */
    if (bitw_put_code(&w, lit_code[256], lit_len[256]) != 0) goto fail;
    if (wubuzip_bitw_finish(&w, out, out_len) != 0) goto fail;
    return 0;

fail:
    wubuzip_bitw_free(&w);
    return -1;
}
