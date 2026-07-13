#include "limitcode.h"

#include <stdlib.h>
#include <string.h>

/* Length-limited Huffman via package-merge (Larmore & Hirschberg).
 *
 * Each symbol is a "coin" of weight w[i]. Standard package-merge builds, for
 * each length limit L in 1..maxlen, a sorted list by repeatedly PACKING the two
 * lightest coins (sum weight) and APPENDING the original n coins. The (2n-1)-th
 * coin of the level-L list is the boundary package; symbols within it get
 * length >= L. A symbol's final length = the count of levels where it is on the
 * boundary. This yields the minimum-redundancy length-limited code.
 *
 * Boundary membership is tracked with a bitset. Our callers need at most
 * 288 (literal/length) + 32 (distance) = 320 symbols, so a 320-bit field (10
 * x 32-bit words) is sufficient and avoids per-symbol heap bitsets. */

#define WBITS_NWORDS 10   /* 320 bits */
typedef struct { uint32_t w[WBITS_NWORDS]; } bits320;

static void bset(uint32_t *b, int i) { b[i >> 5] |= (uint32_t)1u << (i & 31); }
static int bget(const uint32_t *b, int i) { return (b[i >> 5] >> (i & 31)) & 1u; }
static void bor(uint32_t *d, const uint32_t *s) { for (int i = 0; i < WBITS_NWORDS; i++) d[i] |= s[i]; }

typedef struct {
    uint64_t w;
    bits320 bits;
} coin_t;

static int coin_cmp(const void *a, const void *b) {
    const coin_t *x = (const coin_t *)a, *y = (const coin_t *)b;
    if (x->w != y->w) return (x->w > y->w) ? 1 : -1;
    return 0;
}

int wubuzip_limit_lengths(const uint32_t *freqs, int n, int maxlen, uint8_t *len) {
    memset(len, 0, (size_t)n * sizeof *len);

    int used = 0;
    for (int i = 0; i < n; i++) if (freqs[i] > 0) used++;
    if (used == 0) return 0;
    if (used == 1) { for (int i = 0; i < n; i++) if (freqs[i] > 0) { len[i] = 1; break; } return 0; }
    if (maxlen < 1) return -1;
    if (n > WBITS_NWORDS * 32) return -1;   /* alphabet too large for bits320 */

    int idx[WBITS_NWORDS * 32];
    bits320 sbits[WBITS_NWORDS * 32];
    int nsym = 0;
    for (int i = 0; i < n; i++) {
        if (freqs[i] > 0) {
            idx[nsym] = i;
            memset(&sbits[nsym], 0, sizeof(bits320));
            bset(sbits[nsym].w, nsym);
            nsym++;
        }
    }

    uint8_t lcount[WBITS_NWORDS * 32];
    memset(lcount, 0, sizeof lcount);

    /* original coins (sorted once) */
    coin_t *orig = malloc((size_t)nsym * sizeof *orig);
    if (!orig) return -1;
    for (int s = 0; s < nsym; s++) {
        orig[s].w = (uint64_t)freqs[idx[s]];
        memset(&orig[s].bits, 0, sizeof(bits320));
        bor(orig[s].bits.w, sbits[s].w);
    }
    qsort(orig, (size_t)nsym, sizeof *orig, coin_cmp);

    for (int L = 1; L <= maxlen; L++) {
        size_t cap = (size_t)nsym * (size_t)(L + 2) + 16;
        coin_t *list = malloc(cap * sizeof *list);
        if (!list) { free(orig); return -1; }
        size_t listn = (size_t)nsym;
        memcpy(list, orig, listn * sizeof *list);

        for (int lvl = 1; lvl <= L; lvl++) {
            qsort(list, listn, sizeof *list, coin_cmp);
            coin_t *pkg = malloc((listn + 1) * sizeof *pkg);
            if (!pkg) { free(list); free(orig); return -1; }
            size_t pn = 0;
            for (size_t i = 0; i + 1 < listn; i += 2) {
                pkg[pn].w = list[i].w + list[i + 1].w;
                memset(&pkg[pn].bits, 0, sizeof(bits320));
                bor(pkg[pn].bits.w, list[i].bits.w);
                bor(pkg[pn].bits.w, list[i + 1].bits.w);
                pn++;
            }
            if (listn & 1) pkg[pn++] = list[listn - 1];
            coin_t *next = malloc((pn + (size_t)nsym + 1) * sizeof *next);
            if (!next) { free(list); free(pkg); free(orig); return -1; }
            size_t nn = 0;
            for (size_t i = 0; i < pn; i++) next[nn++] = pkg[i];
            for (int s = 0; s < nsym; s++) next[nn++] = orig[s];
            free(list);
            free(pkg);
            list = next;
            listn = nn;
        }

        qsort(list, listn, sizeof *list, coin_cmp);
        int bnd = 2 * (nsym - 1);
        if (bnd < (int)listn) {
            for (int s = 0; s < nsym; s++)
                if (bget(list[bnd].bits.w, s)) lcount[s]++;
        }
        free(list);
    }

    free(orig);

    for (int s = 0; s < nsym; s++) {
        int L = lcount[s];
        if (L < 1) L = 1;
        if (L > maxlen) L = maxlen;
        len[idx[s]] = (uint8_t)L;
    }
    return 0;
}
