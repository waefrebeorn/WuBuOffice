#include "canon.h"

#include <stdlib.h>
#include <string.h>

/* Canonical Huffman construction (RFC 1951 3.2.2): within a given length the
 * codes are consecutive; the first code of length L is the previous first code
 * shifted left by one. We build the code *lengths* via the classic
 * frequency-ordered Huffman algorithm (min-heap on frequencies), then assign
 * canonical codes. */

typedef struct {
    uint32_t freq;
    int symbol;   /* -1 = internal node */
    int parent;
    uint8_t depth;
} node_t;

static int node_cmp(const void *a, const void *b) {
    const node_t *x = (const node_t *)a, *y = (const node_t *)b;
    if (x->freq != y->freq) return (x->freq > y->freq) ? 1 : -1;
    /* tie-break by symbol to keep the tree construction deterministic */
    return (x->symbol > y->symbol) ? 1 : -1;
}

int wubuzip_canon(const uint32_t *freqs, int n, int maxlen,
                  uint16_t *code, uint8_t *len) {
    memset(code, 0, (size_t)n * sizeof *code);
    memset(len, 0, (size_t)n * sizeof *len);

    int used = 0;
    for (int i = 0; i < n; i++) if (freqs[i] > 0) used++;
    if (used == 0) return 0; /* nothing to code */

    /* Build a Huffman tree over at most n nodes (leaves + internal). */
    node_t *nd = calloc((size_t)(2 * n), sizeof *nd);
    if (!nd) return -1;

    int nn = 0;
    for (int i = 0; i < n; i++) {
        if (freqs[i] > 0) {
            nd[nn].freq = freqs[i];
            nd[nn].symbol = i;
            nd[nn].parent = -1;
            nd[nn].depth = 0;
            nn++;
        }
    }

    /* Single symbol: assign a 1-bit code so it is decodable. */
    if (nn == 1) {
        int sym = nd[0].symbol;
        len[sym] = 1;
        code[sym] = 0;
        free(nd);
        return 0;
    }

    /* Repeatedly merge the two lowest-frequency nodes. */
    int nodes = nn;
    while (nodes > 1) {
        /* find the two smallest-frequency active (parent == -1) nodes */
        int a = -1;
        for (int i = 0; i < nodes; i++) {
            if (nd[i].parent != -1) continue;
            if (a == -1 || node_cmp(&nd[i], &nd[a]) < 0) a = i;
        }
        int b = -1;
        for (int i = 0; i < nodes; i++) {
            if (nd[i].parent != -1 || i == a) continue;
            if (b == -1 || node_cmp(&nd[i], &nd[b]) < 0) b = i;
        }
        /* a must exist; if b is -1 the tree is malformed (shouldn't happen) */
        if (b == -1) { free(nd); return -1; }
        nd[a].parent = nodes;
        nd[b].parent = nodes;
        nd[nodes].freq = nd[a].freq + nd[b].freq;
        nd[nodes].symbol = -1;
        nd[nodes].parent = -1;
        nd[nodes].depth = 0;
        nodes++;
    }
    int root = nodes - 1;
    (void)root;

    /* Compute leaf depths -> code lengths. */
    int maxdepth = 0;
    for (int i = 0; i < nn; i++) {
        int d = 0, cur = i;
        while (nd[cur].parent != -1) { d++; cur = nd[cur].parent; }
        int sym = nd[i].symbol;
        len[sym] = (uint8_t)d;
        if (d > maxdepth) maxdepth = d;
    }

    if (maxdepth > maxlen) { free(nd); return -1; }

    /* Assign canonical codes. */
    int cnt[16], offs[16];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) cnt[len[i]]++;
    cnt[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + cnt[i];

    uint16_t c[16];
    c[0] = 0;
    for (int i = 1; i <= 15; i++) c[i] = (uint16_t)((c[i - 1] + cnt[i - 1]) << 1);

    for (int i = 0; i < n; i++) {
        int L = len[i];
        if (L == 0) continue;
        code[i] = c[L]++;
    }

    free(nd);
    return 0;
}
