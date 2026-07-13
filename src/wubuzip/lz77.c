/* WuBuOffice -- wubuzip/lz77
 * Greedy + lazy LZ77 matcher for the DEFLATE encoder.
 *
 * Clean-room, from-scratch (SLERM): no zlib source, no third-party code.
 * System libz is used ONLY by the test suite as a format-validation oracle.
 *
 * Finds (length, distance) tokens over a 3-byte hash-chain window. Lazy
 * evaluation delays emitting a literal when a longer match is available at the
 * next position, improving compression at little cost. */

#include "lz77.h"

#include <stdlib.h>
#include <string.h>

struct wubuzip_lz {
    wubuzip_lz_tok *tok;
    size_t n, cap;
    const uint8_t *in;
    size_t in_len;
};

struct hashentry {
    int32_t head;          /* most recent position, or -1 */
    int32_t prev[1];       /* chain, sized WIN dynamically via per-pos array */
};

/* We keep a per-position `prev` chain in a flat array `chain[pos]`. */
static uint32_t hash3(const uint8_t *p) {
    uint32_t h = 0;
    h = (h * 2654435761u) ^ p[0];
    h = (h * 2654435761u) ^ p[1];
    h = (h * 2654435761u) ^ p[2];
    return (h >> (32 - WUBUZIP_LZ_HASH_BITS)) & (WUBUZIP_LZ_HASH_SIZE - 1);
}

static void push_tok(wubuzip_lz *lz, const wubuzip_lz_tok *t) {
    if (lz->n == lz->cap) {
        lz->cap = lz->cap ? lz->cap * 2 : 1024;
        wubuzip_lz_tok *nt = realloc(lz->tok, lz->cap * sizeof *nt);
        if (nt) lz->tok = nt;
    }
    lz->tok[lz->n++] = *t;
}

/* Longest match starting at `pos`, searching the hash chain. Returns length
 * (>=3) and sets *dist. Returns 0 if no match of length >= 3. */
static int longest(const uint8_t *in, size_t in_len,
                   const int32_t *head, const int32_t *chain,
                   size_t pos, int *dist_out) {
    uint32_t h = hash3(in + pos);
    int32_t cand = head[h];
    int best = 0, best_dist = 0;
    int steps = 0;
    while (cand >= 0 && steps < WUBUZIP_LZ_CHAIN) {
        size_t j = (size_t)cand;
        if ((size_t)pos - j > WUBUZIP_LZ_WIN) break;
        if (in[j] == in[pos]) {
            int run = 0;
            size_t maxrun = in_len - pos;
            if (maxrun > WUBUZIP_LZ_NICE) maxrun = WUBUZIP_LZ_NICE;
            while ((size_t)run < maxrun && in[j + run] == in[pos + run]) run++;
            if (run > best) {
                best = run;
                best_dist = (int)(pos - j);
                if (best >= WUBUZIP_LZ_NICE) break;
            }
        }
        cand = chain[j];
        steps++;
    }
    *dist_out = best_dist;
    return best;
}

int wubuzip_lz_parse(const uint8_t *in, size_t in_len, wubuzip_lz **out) {
    wubuzip_lz *lz = calloc(1, sizeof *lz);
    if (!lz) return -1;
    lz->in = in;
    lz->in_len = in_len;

    int32_t *head = malloc(WUBUZIP_LZ_HASH_SIZE * sizeof *head);
    int32_t *chain = in_len ? malloc(in_len * sizeof *chain) : NULL;
    if (!head || (in_len && !chain)) { free(head); free(chain); free(lz); return -1; }
    for (uint32_t i = 0; i < WUBUZIP_LZ_HASH_SIZE; i++) head[i] = -1;

    size_t pos = 0;
    while (pos < in_len) {
        if (in_len - pos < 3) {
            /* emit remaining as literals */
            wubuzip_lz_tok t = {0}; t.kind = 0; t.lit = in[pos];
            push_tok(lz, &t); pos++;
            continue;
        }
        int d1 = 0, d2 = 0;
        int l1 = longest(in, in_len, head, chain, pos, &d1);
        int l2 = 0;
        if (l1 >= 3 && pos + 1 < in_len - 2)
            l2 = longest(in, in_len, head, chain, pos + 1, &d2);

        if (l1 >= 3 && l1 >= l2) {
            wubuzip_lz_tok t = {0};
            t.kind = 1; t.len = (uint16_t)l1; t.dist = (uint16_t)d1;
            push_tok(lz, &t);
            /* advance and update hash chain for every position consumed */
            for (int k = 0; k < l1; k++) {
                if (pos + (size_t)k + 3 <= in_len) {
                    uint32_t h = hash3(in + pos + k);
                    chain[pos + k] = head[h];
                    head[h] = (int32_t)(pos + k);
                }
            }
            pos += (size_t)l1;
        } else if (l2 >= 3) {
            /* lazy: emit literal at pos, then the longer match at pos+1 */
            wubuzip_lz_tok t = {0};
            t.kind = 0; t.lit = in[pos];
            push_tok(lz, &t);
            if (pos + 3 <= in_len) {
                uint32_t h = hash3(in + pos);
                chain[pos] = head[h];
                head[h] = (int32_t)pos;
            }
            pos++;
            /* next iteration will find l2; but emit it now to keep chain state */
            wubuzip_lz_tok m = {0};
            m.kind = 1; m.len = (uint16_t)l2; m.dist = (uint16_t)d2;
            push_tok(lz, &m);
            for (int k = 0; k < l2; k++) {
                if (pos + (size_t)k + 3 <= in_len) {
                    uint32_t h = hash3(in + pos + k);
                    chain[pos + k] = head[h];
                    head[h] = (int32_t)(pos + k);
                }
            }
            pos += (size_t)l2;
        } else {
            wubuzip_lz_tok t = {0};
            t.kind = 0; t.lit = in[pos];
            push_tok(lz, &t);
            if (pos + 3 <= in_len) {
                uint32_t h = hash3(in + pos);
                chain[pos] = head[h];
                head[h] = (int32_t)pos;
            }
            pos++;
        }
    }

    free(head);
    free(chain);
    *out = lz;
    return 0;
}

const wubuzip_lz_tok *wubuzip_lz_tokens(const wubuzip_lz *lz) { return lz->tok; }
size_t wubuzip_lz_count(const wubuzip_lz *lz) { return lz->n; }
void wubuzip_lz_free(wubuzip_lz *lz) {
    if (!lz) return;
    free(lz->tok);
    free(lz);
}
