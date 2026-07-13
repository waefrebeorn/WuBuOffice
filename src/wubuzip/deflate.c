#include "deflate.h"
#include "bitw.h"
#include "canon.h"
#include "limitcode.h"
#include "fixedcode.h"
#include "block.h"
#include "lz77.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIT_SYMS 288
#define DIST_SYMS 32
#define MAX_MATCH 258
#define BLK_SIZE 32768   /* max input per DEFLATE block (bounds LZ77 memory) */

/* Emit a Huffman code MSB-first into the LSB-first stream (RFC 1951 3.1.1). */
static int put_code(wubuzip_bitwriter *w, uint32_t v, int n) {
    for (int k = n - 1; k >= 0; k--)
        if (wubuzip_bitw_put(w, (v >> k) & 1u, 1) != 0) return -1;
    return 0;
}

/* RLE-encode a run of code lengths using the 3-bit codelen alphabet. */
static int emit_lengths(wubuzip_bitwriter *w, const uint8_t *lens, int n,
                        const uint16_t *clcode, const uint8_t *cllen) {
    int i = 0;
    while (i < n) {
        int v = lens[i];
        if (v == 0) {
            int run = 1;
            while (i + run < n && lens[i + run] == 0 && run < 138) run++;
            if (run >= 11) {
                if (put_code(w, clcode[18], cllen[18]) != 0) return -1;
                if (wubuzip_bitw_put(w, (uint32_t)(run - 11), 7) != 0) return -1;
            } else if (run >= 3) {
                if (put_code(w, clcode[17], cllen[17]) != 0) return -1;
                if (wubuzip_bitw_put(w, (uint32_t)(run - 3), 3) != 0) return -1;
            } else {
                if (put_code(w, clcode[0], cllen[0]) != 0) return -1;
            }
            i += run;
        } else {
            if (put_code(w, clcode[v], cllen[v]) != 0) return -1;
            int run = 1;
            while (i + run < n && lens[i + run] == v && run < 6) run++;
            if (run >= 3) {
                if (put_code(w, clcode[16], cllen[16]) != 0) return -1;
                if (wubuzip_bitw_put(w, (uint32_t)(run - 3), 2) != 0) return -1;
                i += run;
            } else {
                i += 1;
            }
        }
    }
    return 0;
}

/* Frequency-weighted cost (bits) of a token stream under given code lengths. */
static size_t cost_tokens(const wubuzip_lz_tok *tok, size_t ntok,
                          const uint8_t *lit_len, const uint8_t *dist_len) {
    size_t bits = 0;
    for (size_t i = 0; i < ntok; i++) {
        if (tok[i].kind == 0) {
            bits += lit_len[tok[i].lit];
        } else {
            int lsym = 0;
            for (int k = 0; k < 29; k++) {
                int hi = wubuzip_len_base[k] + (1 << wubuzip_len_extra[k]) - 1;
                if (tok[i].len >= wubuzip_len_base[k] && tok[i].len <= hi) { lsym = 257 + k; break; }
            }
            bits += lit_len[lsym] + wubuzip_len_extra[lsym];
            int dsym = 0;
            for (int k = 0; k < 30; k++) {
                int hi = wubuzip_dist_base[k] + (1 << wubuzip_dist_extra[k]) - 1;
                if (tok[i].dist >= wubuzip_dist_base[k] && tok[i].dist <= hi) { dsym = k; break; }
            }
            bits += dist_len[dsym] + wubuzip_dist_extra[dsym];
        }
    }
    return bits;
}

/* Emit one block. method: 0=stored, 1=fixed, 2=dynamic. bfinal sets BFINAL. */
static int emit_block(wubuzip_bitwriter *w, const uint8_t *in, size_t s, size_t e,
                      int bfinal, int method) {
    size_t blen = e - s;
    if (blen == 0) { /* empty block: emit BFINAL+BTYPE(fixed)+end-of-block */
        if (wubuzip_bitw_put(w, (uint32_t)bfinal, 1) != 0) return -1;
        if (wubuzip_bitw_put(w, 1, 2) != 0) return -1;
        uint16_t lc[288]; uint8_t ll[288];
        uint16_t dc[32]; uint8_t dl[32];
        wubuzip_fixed_code_init(lc, ll, dc, dl);
        return put_code(w, lc[256], ll[256]);
    }
    if (wubuzip_bitw_put(w, (uint32_t)bfinal, 1) != 0) return -1;
    if (wubuzip_bitw_put(w, (uint32_t)method & 3u, 2) != 0) return -1;

    if (method == 0) { /* stored */
        wubuzip_bitw_align(w);
        uint32_t n = (uint32_t)blen;
        if (wubuzip_bitw_byte(w, (uint8_t)(n & 0xFF)) != 0) return -1;
        if (wubuzip_bitw_byte(w, (uint8_t)((n >> 8) & 0xFF)) != 0) return -1;
        if (wubuzip_bitw_byte(w, (uint8_t)(~n & 0xFF)) != 0) return -1;
        if (wubuzip_bitw_byte(w, (uint8_t)((~n >> 8) & 0xFF)) != 0) return -1;
        for (size_t i = 0; i < blen; i++)
            if (wubuzip_bitw_byte(w, in[s + i]) != 0) return -1;
        return 0;
    }

    /* Build tokens for this block. */
    wubuzip_lz *lz = NULL;
    if (wubuzip_lz_parse(in + s, blen, &lz) != 0) return -1;
    const wubuzip_lz_tok *tok = wubuzip_lz_tokens(lz);
    size_t ntok = wubuzip_lz_count(lz);

    if (method == 1) { /* fixed */
        uint16_t lc[LIT_SYMS], dc[DIST_SYMS]; uint8_t ll[LIT_SYMS], dl[DIST_SYMS];
        wubuzip_fixed_code_init(lc, ll, dc, dl);
        for (size_t i = 0; i < ntok; i++) {
            if (tok[i].kind == 0) {
                if (put_code(w, lc[tok[i].lit], ll[tok[i].lit]) != 0) goto fail;
            } else {
                int lsym = 0;
                for (int k = 0; k < 29; k++) {
                    int hi = wubuzip_len_base[k] + (1 << wubuzip_len_extra[k]) - 1;
                    if (tok[i].len >= wubuzip_len_base[k] && tok[i].len <= hi) { lsym = 257 + k; break; }
                }
                if (put_code(w, lc[lsym], ll[lsym]) != 0) goto fail;
                if (wubuzip_len_extra[lsym - 257])
                    if (wubuzip_bitw_put(w, (uint32_t)(tok[i].len - wubuzip_len_base[lsym - 257]), wubuzip_len_extra[lsym - 257]) != 0) goto fail;
                int dsym = 0;
                for (int k = 0; k < 30; k++) {
                    int hi = wubuzip_dist_base[k] + (1 << wubuzip_dist_extra[k]) - 1;
                    if (tok[i].dist >= wubuzip_dist_base[k] && tok[i].dist <= hi) { dsym = k; break; }
                }
                if (put_code(w, dc[dsym], dl[dsym]) != 0) goto fail;
                if (wubuzip_dist_extra[dsym])
                    if (wubuzip_bitw_put(w, (uint32_t)(tok[i].dist - wubuzip_dist_base[dsym]), wubuzip_dist_extra[dsym]) != 0) goto fail;
            }
        }
        if (put_code(w, lc[256], ll[256]) != 0) goto fail; /* end of block */
        wubuzip_lz_free(lz);
        return 0;
    }

    /* method == 2 : dynamic */
    {
        uint32_t lfreq[LIT_SYMS]; uint32_t dfreq[DIST_SYMS];
        memset(lfreq, 0, sizeof lfreq);
        memset(dfreq, 0, sizeof dfreq);
        int maxlit = 255, maxdist = 0;
        for (size_t i = 0; i < ntok; i++) {
            if (tok[i].kind == 0) {
                lfreq[tok[i].lit]++;
                if ((int)tok[i].lit > maxlit) maxlit = tok[i].lit;
            } else {
                int lsym = 0;
                for (int k = 0; k < 29; k++) {
                    int hi = wubuzip_len_base[k] + (1 << wubuzip_len_extra[k]) - 1;
                    if (tok[i].len >= wubuzip_len_base[k] && tok[i].len <= hi) { lsym = 257 + k; break; }
                }
                lfreq[lsym]++;
                if (lsym > maxlit) maxlit = lsym;
                int dsym = 0;
                for (int k = 0; k < 30; k++) {
                    int hi = wubuzip_dist_base[k] + (1 << wubuzip_dist_extra[k]) - 1;
                    if (tok[i].dist >= wubuzip_dist_base[k] && tok[i].dist <= hi) { dsym = k; break; }
                }
                dfreq[dsym]++;
                if (dsym > maxdist) maxdist = dsym;
            }
        }
        lfreq[256]++; /* end-of-block symbol always present */

        int hlit = maxlit + 1; if (hlit < 257) hlit = 257;
        int hdist = maxdist + 1; if (hdist < 1) hdist = 1;

        uint8_t llen[LIT_SYMS], dlen[DIST_SYMS];
        uint16_t lcode[LIT_SYMS], dcode[DIST_SYMS];
        if (wubuzip_limit_lengths(lfreq, hlit, 15, llen) != 0) goto fail;
        if (wubuzip_limit_lengths(dfreq, hdist, 15, dlen) != 0) goto fail;
        /* canonical codes from the chosen lengths */
        {
            uint32_t w[LIT_SYMS];
            for (int i = 0; i < hlit; i++) w[i] = llen[i] ? 1u : 0u;
            wubuzip_canon(w, hlit, 15, lcode, llen);
            uint32_t wd[DIST_SYMS];
            for (int i = 0; i < hdist; i++) wd[i] = dlen[i] ? 1u : 0u;
            wubuzip_canon(wd, hdist, 15, dcode, dlen);
        }

        /* Combined length array (hlit + hdist) for the codelen code. */
        uint8_t comb[LIT_SYMS + DIST_SYMS];
        memcpy(comb, llen, (size_t)hlit);
        memcpy(comb + hlit, dlen, (size_t)hdist);

        static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
        uint8_t cllen19[19]; uint32_t clfreq[19];
        memset(cllen19, 0, sizeof cllen19);
        memset(clfreq, 0, sizeof clfreq);
        for (int i = 0; i < hlit + hdist; i++) clfreq[comb[i]]++;
        /* build the codelen code lengths (length-limited to 7) */
        if (wubuzip_limit_lengths(clfreq, 19, 7, cllen19) != 0) goto fail;
        uint16_t clcode[19];
        { uint32_t w[19]; for (int i = 0; i < 19; i++) w[i] = cllen19[i] ? 1u : 0u;
          wubuzip_canon(w, 19, 7, clcode, cllen19); }

        /* header */
        if (wubuzip_bitw_put(w, (uint32_t)(hlit - 257), 5) != 0) goto fail;
        if (wubuzip_bitw_put(w, (uint32_t)(hdist - 1), 5) != 0) goto fail;
        int hclen = 18;
        while (hclen >= 4 && cllen19[ORD[hclen]] == 0) hclen--;
        if (wubuzip_bitw_put(w, (uint32_t)(hclen - 3), 4) != 0) goto fail;
        for (int i = 0; i <= hclen; i++)
            if (wubuzip_bitw_put(w, cllen19[ORD[i]], 3) != 0) goto fail;

        /* emit combined code lengths via RLE */
        if (emit_lengths(w, comb, hlit + hdist, clcode, cllen19) != 0) goto fail;

        /* emit tokens */
        for (size_t i = 0; i < ntok; i++) {
            if (tok[i].kind == 0) {
                if (put_code(w, lcode[tok[i].lit], llen[tok[i].lit]) != 0) goto fail;
            } else {
                int lsym = 0;
                for (int k = 0; k < 29; k++) {
                    int hi = wubuzip_len_base[k] + (1 << wubuzip_len_extra[k]) - 1;
                    if (tok[i].len >= wubuzip_len_base[k] && tok[i].len <= hi) { lsym = 257 + k; break; }
                }
                if (put_code(w, lcode[lsym], llen[lsym]) != 0) goto fail;
                if (wubuzip_len_extra[lsym - 257])
                    if (wubuzip_bitw_put(w, (uint32_t)(tok[i].len - wubuzip_len_base[lsym - 257]), wubuzip_len_extra[lsym - 257]) != 0) goto fail;
                int dsym = 0;
                for (int k = 0; k < 30; k++) {
                    int hi = wubuzip_dist_base[k] + (1 << wubuzip_dist_extra[k]) - 1;
                    if (tok[i].dist >= wubuzip_dist_base[k] && tok[i].dist <= hi) { dsym = k; break; }
                }
                if (put_code(w, dcode[dsym], dlen[dsym]) != 0) goto fail;
                if (wubuzip_dist_extra[dsym])
                    if (wubuzip_bitw_put(w, (uint32_t)(tok[i].dist - wubuzip_dist_base[dsym]), wubuzip_dist_extra[dsym]) != 0) goto fail;
            }
        }
        if (put_code(w, lcode[256], llen[256]) != 0) goto fail; /* end of block */
        wubuzip_lz_free(lz);
        return 0;
    }

fail:
    wubuzip_lz_free(lz);
    return -1;
}

/* Pick the cheapest block method by estimating bit cost. */
static int best_method(const uint8_t *in, size_t s, size_t e) {
    size_t blen = e - s;
    if (blen == 0) return 1; /* fixed empty */
    wubuzip_lz *lz = NULL;
    if (wubuzip_lz_parse(in + s, blen, &lz) != 0) return 1;
    const wubuzip_lz_tok *tok = wubuzip_lz_tokens(lz);
    size_t ntok = wubuzip_lz_count(lz);

    /* stored cost (bits): 3 header + align pad + 32 len + 32 nlen + 8*blen */
    size_t stored_cost = 3 + 32 + 32 + 8 * blen; /* align pad bounded <=5, ignore */

    uint16_t lc[LIT_SYMS], dc[DIST_SYMS]; uint8_t ll[LIT_SYMS], dl[DIST_SYMS];
    wubuzip_fixed_code_init(lc, ll, dc, dl);
    size_t fixed_cost = 3 + cost_tokens(tok, ntok, ll, dl) + ll[256];

    /* dynamic cost estimate: header overhead + token cost */
    uint32_t lfreq[LIT_SYMS]; uint32_t dfreq[DIST_SYMS];
    memset(lfreq, 0, sizeof lfreq); memset(dfreq, 0, sizeof dfreq);
    int maxlit = 255, maxdist = 0;
    for (size_t i = 0; i < ntok; i++) {
        if (tok[i].kind == 0) { lfreq[tok[i].lit]++; if ((int)tok[i].lit > maxlit) maxlit = tok[i].lit; }
        else {
            int lsym = 0;
            for (int k = 0; k < 29; k++) {
                int hi = wubuzip_len_base[k] + (1 << wubuzip_len_extra[k]) - 1;
                if (tok[i].len >= wubuzip_len_base[k] && tok[i].len <= hi) { lsym = 257 + k; break; }
            }
            lfreq[lsym]++; if (lsym > maxlit) maxlit = lsym;
            int dsym = 0;
            for (int k = 0; k < 30; k++) {
                int hi = wubuzip_dist_base[k] + (1 << wubuzip_dist_extra[k]) - 1;
                if (tok[i].dist >= wubuzip_dist_base[k] && tok[i].dist <= hi) { dsym = k; break; }
            }
            dfreq[dsym]++; if (dsym > maxdist) maxdist = dsym;
        }
    }
    lfreq[256]++;
    int hlit = maxlit + 1; if (hlit < 257) hlit = 257;
    int hdist = maxdist + 1; if (hdist < 1) hdist = 1;
    uint8_t llen[LIT_SYMS], dlen[DIST_SYMS];
    wubuzip_limit_lengths(lfreq, hlit, 15, llen);
    wubuzip_limit_lengths(dfreq, hdist, 15, dlen);
    size_t dyn_cost = 3 + 5 + 5 + 4; /* header fields */
    dyn_cost += cost_tokens(tok, ntok, llen, dlen) + llen[256];
    /* crude codelen header overhead (~ (hlit+hdist)/2 * 4 bits) */
    dyn_cost += (size_t)(hlit + hdist) * 2;

    wubuzip_lz_free(lz);

    if (stored_cost <= fixed_cost && stored_cost <= dyn_cost) return 0;
    if (dyn_cost <= fixed_cost) return 2;
    return 1;
}

int wubuzip_deflate(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
    wubuzip_bitwriter w;
    wubuzip_bitw_init(&w);

    if (in_len == 0) {
        if (emit_block(&w, in, 0, 0, 1, 1) != 0) goto fail;
        return wubuzip_bitw_finish(&w, out, out_len);
    }

    size_t s = 0;
    while (s < in_len) {
        size_t e = s + BLK_SIZE; if (e > in_len) e = in_len;
        int last = (e == in_len);
        int method = best_method(in, s, e);
        if (emit_block(&w, in, s, e, last ? 1 : 0, method) != 0) goto fail;
        s = e;
    }
    return wubuzip_bitw_finish(&w, out, out_len);

fail:
    wubuzip_bitw_free(&w);
    return -1;
}
