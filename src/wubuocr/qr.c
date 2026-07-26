/* qr.c -- self-contained QR-code codec (encoder + decoder), clean C11, no deps.
 *
 * Scope: byte-mode QR codes, ECC level M, versions 1..7 (covers ~14..122 bytes
 * of text, the common case for "scan this to get a URL/string"). Full pipeline:
 *   encoder: text -> bitstream -> RS ECC -> interleave -> module placement ->
 *            masking -> format/version info -> module matrix.
 *   decoder: finder-pattern acquisition (3 corners, affine transform) -> module
 *            sampling -> unmask -> de-interleave -> RS error correction ->
 *            segment parse -> UTF-8 text.
 *
 * The decoder is robust to axis-aligned (and mildly skewed) scans. A downstream
 * caller (crnn_transcribe) emits a "kind":"qr" block with the decoded text.
 *
 * References: ISO/IEC 18004 (QR Code) module placement, format/version info, and
 * GF(256) Reed-Solomon over the primitive polynomial 0x11d.
 */
#include "qr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* ---------------- GF(256) over x^8 + x^4 + x^3 + x^2 + 1 (0x11d) ---------------- */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static void gf_init(void){
    int x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11d;
    }
    for (int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
}
static inline uint8_t gf_mul(uint8_t a, uint8_t b){
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}
/* polynomial multiply: r[k] = sum_i p[i]*q[k-i]; p,q,r are const-first
 * (r[0]=x^0 coeff ... r[lp+lq-2]=x^(lp+lq-2) coeff) */
static void gf_poly_mul(const uint8_t *p, int lp, const uint8_t *q, int lq, uint8_t *r){
    for (int i = 0; i < lp + lq - 1; i++) r[i] = 0;
    for (int j = 0; j < lq; j++)
        for (int i = 0; i < lp; i++)
            r[i + j] ^= gf_mul(p[i], q[j]);
}
/* generator polynomial of degree nroots for RS; const-first:
 * gen[0]=x^0 (const) ... gen[nroots]=x^nroots (leading=1).
 * Roots are alpha^0 .. alpha^(nroots-1). */
static void rs_gen_poly(int nroots, uint8_t *gen){
    uint8_t g[64] = {0}; g[0] = 1; int lg = 1;
    for (int i = 0; i < nroots; i++) {
        uint8_t q[2] = { gf_exp[i], 1 };   /* (x - alpha^i) const-first: [alpha^i, 1] */
        uint8_t ng[64];
        gf_poly_mul(g, lg, q, 2, ng);
        memcpy(g, ng, (size_t)lg + 1);
        lg++;
    }
    memcpy(gen, g, (size_t)nroots + 1);
}
/* append nroots ECC bytes to `data` (ndata) -> result in `out` (nroots bytes) */
static void rs_encode(const uint8_t *data, int ndata, int nroots, uint8_t *out){
    uint8_t gen[64];
    rs_gen_poly(nroots, gen);
    uint8_t *buf = (uint8_t*)malloc((size_t)(ndata + nroots));
    for (int i = 0; i < ndata; i++) buf[i] = data[i];
    for (int i = 0; i < nroots; i++) buf[ndata + i] = 0;
    for (int i = 0; i < ndata; i++) {
        uint8_t coef = buf[i];
        if (coef != 0) {
            for (int j = 0; j < nroots; j++)
                buf[i + 1 + j] ^= gf_mul(gen[nroots - 1 - j], coef);
        }
    }
    for (int i = 0; i < nroots; i++) out[i] = buf[ndata + i];
    free(buf);
}
/* Berlekamp-Massey + Forney decode; corrects `recv` in place. Returns 0 if OK.
 * Convention (matches rs_encode): recv[0] is the HIGHEST power (x^(n-1)),
 * so syndrome S_i = sum_j recv[j] * alpha^(i*(n-1-j)).
 * Error at array index p has locator X = alpha^(n-1-p). */
static void rs_pmul_mod(const uint8_t *a, int la, const uint8_t *b, int lb, uint8_t *r, int nsym){
    uint8_t t[128];
    for (int i = 0; i < nsym; i++) t[i] = 0;
    for (int i = 0; i < la; i++)
        for (int j = 0; j < lb && i + j < nsym; j++)
            t[i + j] ^= gf_mul(a[i], b[j]);
    for (int i = 0; i < nsym; i++) r[i] = t[i];
}
int rs_decode(uint8_t *recv, int n, int nroots){
    uint8_t syn[64];
    int nsyn = 0;
    for (int i = 0; i < nroots; i++) {
        uint8_t s = 0;
        for (int j = 0; j < n; j++)
            s ^= gf_mul(recv[j], gf_exp[(i * (n - 1 - j)) % 255]);
        syn[nsyn++] = s;
    }
    int nonzero = 0; for (int i = 0; i < nroots; i++) if (syn[i]) nonzero = 1;
    if (!nonzero) return 0; /* no errors */
    /* Berlekamp-Massey (const-first, no erasures). We keep the locator as
     * Lrev (const-first): Lambda(x) = sum_i Lrev[i] x^i. The x^m shift on Brev
     * is done explicitly via the (i+m) offset. */
    uint8_t Lrev[64]; memset(Lrev, 0, sizeof Lrev); Lrev[0] = 1;
    uint8_t Brev[64]; memset(Brev, 0, sizeof Brev); Brev[0] = 1;
    int Ldeg = 0;   /* degree of current locator == number of errors */
    int m = 1;      /* steps since last update */
    uint8_t b = 1;  /* previous discrepancy (b^{-1} used at update) */
    for (int nn = 0; nn < nroots; nn++) {
        uint8_t delta = syn[nn];
        for (int i = 1; i <= Ldeg; i++) delta ^= gf_mul(Lrev[i], syn[nn - i]);
        if (delta != 0) {
            uint8_t T[64]; memcpy(T, Lrev, sizeof T);
            uint8_t coef = gf_mul(delta, gf_exp[(255 - gf_log[b]) % 255]); /* delta * b^{-1} */
            uint8_t scaled[64]; memset(scaled, 0, sizeof scaled);
            for (int i = 0; i < 64 - m; i++) if (i + m < 64) scaled[i + m] = gf_mul(Brev[i], coef);
            for (int i = 0; i < 64; i++) Lrev[i] ^= scaled[i];
            if (2 * Ldeg <= nn) {
                Ldeg = nn + 1 - Ldeg;
                memcpy(Brev, T, sizeof Brev);
                b = delta;
                m = 1;
            } else {
                m++;
            }
        } else {
            m++;
        }
    }
    int L = Ldeg; /* number of errors */
    /* Chien search: find roots of Lambda(x) at x = alpha^i. In const-first,
     * Lambda(x) = sum_i Lrev[i] x^i. A root at x = alpha^i satisfies
     * alpha^i = X_k^{-1} = alpha^{-(n-1-p)}, so i = -(n-1-p) mod 255, i.e.
     * p = (i + n - 1) mod 255. Include i=0 (X_k = 1, last codeword coeff). */
    int errs[64], nerr = 0;
    for (int i = 0; i < 256; i++) {
        uint8_t ev = 0;
        for (int j = 0; j <= L; j++) ev ^= gf_mul(Lrev[j], gf_exp[(j * i) % 255]);
        if (ev == 0) {
            int p = (i + n - 1) % 255;
            if (p >= 0 && p < n) { errs[nerr++] = p; if (nerr >= 64) break; }
        }
    }
    if (nerr != L) return -1; /* too many errors */
    /* Forney: error at p has locator X_k = alpha^(n-1-p).
     * Omega(x) = (S(x) * Lambda(x)) mod x^nroots (const-first).
     * e_k = X_k * Omega(X_k^{-1}) / Lambda'(X_k^{-1}).
     * Lambda'(x) = sum_j j*Lambda[j] x^(j-1); the factor j is a GF constant,
     * so Lambda'(X_k^{-1}) = sum_{odd j} Lambda[j] * (X_k^{-1})^(j-1). */
    for (int e = 0; e < nerr; e++) {
        int p = errs[e];
        uint8_t X = gf_exp[(n - 1 - p) % 255];
        uint8_t Xinv = gf_exp[(255 - gf_log[X]) % 255];
        uint8_t Om[64]; rs_pmul_mod(syn, nroots, Lrev, L + 1, Om, nroots);
        uint8_t omega = 0;
        for (int i = 0; i < nroots; i++) omega ^= gf_mul(Om[i], gf_exp[(i * (255 - gf_log[X])) % 255]);
        uint8_t deriv = 0;
        for (int j = 1; j <= L; j += 2)
            deriv ^= gf_mul(Lrev[j], gf_exp[((j - 1) * (255 - gf_log[X])) % 255]);
        if (deriv == 0) return -1;
        uint8_t mag = gf_mul(gf_mul(X, omega), gf_exp[(255 - gf_log[deriv]) % 255]);
        recv[p] ^= mag;
    }
    return 0;
}

/* ---------------- QR version tables (ECC level M, versions 1..7) ---------------- */
typedef struct { int ver; int size; int align[3]; int nblocks; int datacw; int ecc; } QRVer;
static const QRVer VERS[7] = {
    {1,21,{},                 1, 16, 10},
    {2,25,{6,18},             1, 28, 16},
    {3,29,{6,22},             1, 44, 26},
    {4,33,{6,26},             2, 32, 18},
    {5,37,{6,30},             2, 43, 22},
    {6,41,{6,34},             4, 27, 16},
    {7,45,{6,22,38},          4, 31, 18},
};
static const int NVER = 7;

static int qr_pick_version(int nbytes){
    /* max encodable bytes ≈ datacw - 2 (mode+len) */
    for (int i = 0; i < NVER; i++) {
        int cap = VERS[i].datacw - 2;
        if (nbytes <= cap) return i;
    }
    return -1;
}

/* module matrix helpers: store as bytes 0/1, plus a 'reserved' map */
typedef struct { int size; uint8_t *m; uint8_t *res; } QRMat;
static QRMat qr_mat_new(int size){
    QRMat q; q.size = size;
    q.m   = (uint8_t*)calloc((size_t)size*size, 1);
    q.res = (uint8_t*)calloc((size_t)size*size, 1);
    return q;
}
static inline void qr_set(QRMat *q, int r, int c, int v, int reserve){
    if (r<0||c<0||r>=q->size||c>=q->size) return;
    q->m[(size_t)r * (size_t)q->size + (size_t)c] = (uint8_t)v;
    if (reserve) q->res[(size_t)r * (size_t)q->size + (size_t)c] = 1;
}
static inline int qr_get(QRMat *q, int r, int c){
    return q->m[(size_t)r * (size_t)q->size + (size_t)c];
}

static void qr_place_finders(QRMat *q){
    /* 7x7 finder at three corners */
    int sz = q->size;
    int corners[3][2] = {{0,0},{0,sz-7},{sz-7,0}};
    for (int k=0;k<3;k++){
        int r0=corners[k][0], c0=corners[k][1];
        for (int dr=0;dr<7;dr++) for (int dc=0;dc<7;dc++){
            int v = 1;
            if (dr==0||dr==6||dc==0||dc==6) v=1;        /* outer ring dark */
            else if (dr>=2&&dr<=4&&dc>=2&&dc<=4) v=1;    /* 3x3 center dark */
            else v=0;                                     /* separator/white ring */
            qr_set(q, r0+dr, c0+dc, v, 1);
        }
        /* separator (white) around finder */
        for (int i=-1;i<=7;i++){
            if (r0+i>=0&&r0+i<sz&&c0-1>=0) qr_set(q,r0+i,c0-1,0,1);
            if (r0+i>=0&&r0+i<sz&&c0+7<sz) qr_set(q,r0+i,c0+7,0,1);
            if (c0+i>=0&&c0+i<sz&&r0-1>=0) qr_set(q,r0-1,c0+i,0,1);
            if (c0+i>=0&&c0+i<sz&&r0+7<sz) qr_set(q,r0+7,c0+i,0,1);
        }
    }
}
static void qr_place_timing(QRMat *q){
    int sz=q->size;
    for (int i=8;i<sz-8;i++){
        int v=(i%2==0)?1:0;
        qr_set(q,6,i,v,1); qr_set(q,i,6,v,1);
    }
}
static void qr_place_alignment(QRMat *q, const QRVer *v){
    if (v->ver==1) return;
    int pts[3]; int np=0;
    for (int i=0;i<3 && v->align[i];i++) pts[np++]=v->align[i];
    for (int i=0;i<np;i++) for (int j=0;j<np;j++){
        int r=pts[i], c=pts[j];
        /* skip if it would overlap a finder pattern */
        if ((r==6&&c==6) || (r==6&&c==pts[np-1]) || (c==6&&r==pts[np-1])) continue;
        for (int dr=-2;dr<=2;dr++) for (int dc=-2;dc<=2;dc++){
            int vv = (dr==-2||dr==2||dc==-2||dc==2||(dr==0&&dc==0))?1:0;
            qr_set(q,r+dr,c+dc,vv,1);
        }
    }
}

/* ---- format information (15 bits): 2-bit ECC level (00=M) + 3-bit mask,
 * BCH(15,5) with generator 0x537, XOR mask 0x5412. ---- */
static uint16_t qr_format_bits(int mask){
    int data = (0 << 3) | (mask & 7);   /* ECC-M = 00 */
    uint16_t v = (uint16_t)(data << 10);
    for (int i = 4; i >= 0; i--)
        if (v & (1u << (i + 10))) v ^= (0x537u << i);
    return (uint16_t)((data << 10) | (v & 0x3FF)) ^ 0x5412u;
}
static void qr_place_format(QRMat *q, int mask){
    uint16_t fmt = qr_format_bits(mask);
    int N = q->size;
    int posA[15][2] = {
        {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,7},{8,8},{7,8},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8}
    };
    for (int i = 0; i < 15; i++)
        qr_set(q, posA[i][0], posA[i][1], (fmt >> i) & 1, 1);
    int posB[15][2] = {
        {N-1,8},{N-2,8},{N-3,8},{N-4,8},{N-5,8},{N-6,8},{N-7,8},
        {8,N-8},{8,N-7},{8,N-6},{8,N-5},{8,N-4},{8,N-3},{8,N-2},{8,N-1}
    };
    for (int i = 0; i < 15; i++)
        qr_set(q, posB[i][0], posB[i][1], (fmt >> i) & 1, 1);
    qr_set(q, N-8, 8, 1, 1);
}

static inline int qr_mask_cond(int mask, int r, int c){
    switch (mask) {
        case 0: return (r + c) % 2 == 0;
        case 1: return r % 2 == 0;
        case 2: return c % 3 == 0;
        case 3: return (r + c) % 3 == 0;
        case 4: return (r / 2 + c / 3) % 2 == 0;
        case 5: return ((r * c) % 2) + ((r * c) % 3) == 0;
        case 6: return (((r * c) % 2) + ((r * c) % 3)) % 2 == 0;
        case 7: return (((r + c) % 2) + ((r * c) % 3)) % 2 == 0;
    }
    return 0;
}

/* Apply mask `mask` to data modules only (function-pattern modules are reserved
 * and never masked). Called once by the encoder with the chosen mask. */
static void qr_apply_mask(QRMat *q, int mask, int undo){
    (void)undo;
    int N = q->size;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
        if (q->res[r * N + c]) continue;
        if (qr_mask_cond(mask, r, c)) q->m[r * N + c] ^= 1;
    }
}

static void qr_place_data(QRMat *q, const uint8_t *cw, int nbytes){
    int N = q->size;
    int bit = 0, total = nbytes * 8;
    int col = N - 1, dir = -1;
    while (col > 0) {
        if (col == 6) col--;
        for (int i = 0; i < N; i++) {
            int r = (dir == -1) ? (N - 1 - i) : i;
            for (int k = 0; k < 2; k++) {
                int c = col - k;
                if (c < 0) continue;
                if (q->res[r * N + c]) continue;
                int b = (bit < total) ? ((cw[bit / 8] >> (7 - (bit % 8))) & 1) : 0;
                q->m[r * N + c] = (uint8_t)b;
                bit++;
            }
        }
        col -= 2; dir = -dir;
    }
}

typedef struct { uint8_t *buf; int nbits; } BitW;
static void bw_put(BitW *bw, int val, int n){
    for (int i = n - 1; i >= 0; i--) {
        int bit = (val >> i) & 1;
        bw->buf[bw->nbits / 8] |= (uint8_t)(bit << (7 - (bw->nbits % 8)));
        bw->nbits++;
    }
}

int qr_encode(const char *text, uint8_t **out, int *out_size){
    static int inited = 0; if (!inited) { gf_init(); inited = 1; }
    int nbytes = (int)strlen(text);
    int vi = qr_pick_version(nbytes);
    if (vi < 0) return -1;
    const QRVer *v = &VERS[vi];
    int N = v->size;
    QRMat q = qr_mat_new(N);
    qr_place_finders(&q);
    qr_place_timing(&q);
    qr_place_alignment(&q, v);
    qr_place_format(&q, 0);

    uint8_t data[2048];
    memset(data, 0, sizeof data);
    BitW bw = { data, 0 };
    bw_put(&bw, 4, 4);
    bw_put(&bw, nbytes, 8);
    for (int i = 0; i < nbytes; i++) bw_put(&bw, (uint8_t)text[i], 8);
    int capacity = v->datacw * 8;
    int term = capacity - bw.nbits; if (term > 4) term = 4;
    for (int i = 0; i < term; i++) bw_put(&bw, 0, 1);
    while (bw.nbits % 8 != 0) bw_put(&bw, 0, 1);
    int data_bytes = bw.nbits / 8;
    uint8_t pad[2] = { 0xEC, 0x11 }; int pi = 0;
    while (data_bytes < v->datacw) { data[data_bytes++] = pad[pi & 1]; pi++; }

    int nblocks = v->nblocks, per = v->datacw / nblocks, ecc = v->ecc;
    uint8_t *eccs = (uint8_t*)malloc((size_t)nblocks * ecc);
    for (int b = 0; b < nblocks; b++)
        rs_encode(data + b * per, per, ecc, eccs + b * ecc);
    int total = v->datacw + nblocks * ecc;
    uint8_t *cw = (uint8_t*)malloc((size_t)total);
    int p = 0;
    for (int i = 0; i < per; i++) for (int b = 0; b < nblocks; b++) cw[p++] = data[b * per + i];
    for (int i = 0; i < ecc; i++) for (int b = 0; b < nblocks; b++) cw[p++] = eccs[b * ecc + i];

    qr_place_data(&q, cw, total);
    /* Apply the chosen mask to data modules (the matrix must be masked so a
     * real decoder's unmask step cancels it). Format info carries mask=0. */
    qr_apply_mask(&q, 0, 0);
    qr_place_format(&q, 0);

    *out = q.m; *out_size = N;
    free(q.res); free(eccs); free(cw);
    return v->ver;
}

int qr_decode_matrix(const uint8_t *matrix, int size, char *out, int outcap){
    static int inited = 0; if (!inited) { gf_init(); inited = 1; }
    if (size < 21 || (size - 17) % 4 != 0) return -1;
    int ver = (size - 17) / 4;
    const QRVer *v = NULL;
    for (int i = 0; i < NVER; i++) if (VERS[i].ver == ver) { v = &VERS[i]; break; }
    if (!v) return -1;

    uint16_t fmt = 0;
    int posA[15][2] = {
        {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,7},{8,8},{7,8},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8}
    };
    for (int i = 0; i < 15; i++) {
        int r = posA[i][0], c = posA[i][1];
        int bit = (r >= 0 && c >= 0 && r < size && c < size) ? matrix[r * size + c] : 0;
        fmt |= (uint16_t)(bit << i);
    }
    fmt ^= 0x5412u;
    int mask = (fmt >> 10) & 7;

    QRMat rq = qr_mat_new(size);
    qr_place_finders(&rq); qr_place_timing(&rq); qr_place_alignment(&rq, v);
    qr_place_format(&rq, mask);
    uint8_t *w = (uint8_t*)malloc((size_t)size * size);
    memcpy(w, matrix, (size_t)size * size);
    for (int r = 0; r < size; r++) for (int c = 0; c < size; c++)
        if (!rq.res[r * size + c] && qr_mask_cond(mask, r, c)) w[r * size + c] ^= 1;

    int nblocks = v->nblocks, per = v->datacw / nblocks, ecc = v->ecc;
    int total = v->datacw + nblocks * ecc;
    uint8_t *cw = (uint8_t*)calloc((size_t)total, 1);
    int bit = 0;
    int col = size - 1, dir = -1;
    while (col > 0) {
        if (col == 6) col--;
        for (int i = 0; i < size; i++) {
            int r = (dir == -1) ? (size - 1 - i) : i;
            for (int k = 0; k < 2; k++) {
                int c = col - k;
                if (c < 0) continue;
                if (rq.res[r * size + c]) continue;
                if (bit < total * 8) {
                    int b = w[r * size + c] & 1;
                    cw[bit / 8] |= (uint8_t)(b << (7 - (bit % 8)));
                    bit++;
                }
            }
        }
        col -= 2; dir = -dir;
    }

    uint8_t *data = (uint8_t*)malloc((size_t)v->datacw);
    uint8_t *eccs = (uint8_t*)malloc((size_t)nblocks * ecc);
    int p = 0;
    for (int i = 0; i < per; i++) for (int b = 0; b < nblocks; b++) data[b * per + i] = cw[p++];
    for (int i = 0; i < ecc; i++) for (int b = 0; b < nblocks; b++) eccs[b * ecc + i] = cw[p++];
    int ok = 0;
    for (int b = 0; b < nblocks; b++) {
        uint8_t *blk = (uint8_t*)malloc((size_t)(per + ecc));
        memcpy(blk, data + b * per, (size_t)per);
        memcpy(blk + per, eccs + b * ecc, (size_t)ecc);
        if (rs_decode(blk, per + ecc, ecc) != 0) ok = -1;
        memcpy(data + b * per, blk, (size_t)per);
        free(blk);
    }
    if (ok == 0) {
        int bi = 0;
        #define GETB() ({ int _b = (data[bi / 8] >> (7 - (bi % 8))) & 1; bi++; _b; })
        int mode = 0; for (int i = 0; i < 4; i++) mode = (mode << 1) | GETB();
        if (mode == 4) {
            int len = 0; for (int i = 0; i < 8; i++) len = (len << 1) | GETB();
            int n = 0;
            for (int i = 0; i < len && n < outcap - 1; i++) {
                int ch = 0; for (int j = 0; j < 8; j++) ch = (ch << 1) | GETB();
                out[n++] = (char)ch;
            }
            out[n] = 0;
        } else { ok = -1; }
        #undef GETB
    }
    free(w); free(rq.m); free(rq.res); free(cw); free(data); free(eccs);
    return ok;
}

/* ---- image acquisition: finder-pattern detection + decode ----
 * Scans the grayscale page for QR finder patterns (the 1:1:3:1:1 dark/light
 * ratio), clusters them into corners, warps the symbol to a module grid via an
 * affine map from the three detected corners, samples the modules, and decodes. */
static int qr_is_dark(unsigned char v, int bg){ return (int)v < (bg + (255 - bg) / 2); }

int qr_detect_blocks(const unsigned char *pix, int W, int H, int bg,
                     int maxn, char text[][256], int txtcap,
                     int *x0, int *y0, int *x1, int *y1){
    (void)txtcap;
    if (W < 21 || H < 21 || maxn <= 0) return 0;
    /* 1) scan rows for the finder profile: 5 runs, colors D L D L D, widths
     *    w0..w4 with w2 ~ 3*w0, w1~w0, w3~w0, w4~w0 (tolerance 40%). */
    typedef struct { int x, y, mw; } Cand;
    Cand *cands = (Cand*)malloc(sizeof(Cand) * (size_t)(W * H / 4 + 16));
    int nc = 0;
    for (int y = 0; y < H; y++) {
        int runs[5], rc = 0, prev = -1, runlen = 0;
        int x = 0;
        while (x < W) {
            int d = qr_is_dark(pix[y * W + x], bg) ? 1 : 0;
            if (d != prev) { if (prev != -1) { runs[rc++] = runlen; if (rc == 5) break; } prev = d; runlen = 1; }
            else runlen++;
            x++;
        }
        if (rc < 5) continue;
        /* runs should be D L D L D => prev sequence; check center is 3x */
        if (runs[2] < 1) continue;
        int w0 = runs[0], w1 = runs[1], w2 = runs[2], w3 = runs[3], w4 = runs[4];
        int okr = (w2 >= 2 * w0) && (w2 <= 5 * w0) &&
                  (w1 <= 2 * w0) && (w1 >= w0 / 2) &&
                  (w3 <= 2 * w0) && (w3 >= w0 / 2) &&
                  (w4 <= 2 * w0) && (w4 >= w0 / 2);
        if (!okr) continue;
        int cx = (w0 + (w0 + w1) + (w0 + w1 + w2)) ; /* approx center col */
        /* center column relative to start of run sequence; recompute precisely */
        int startx = 0; /* we lost start; instead verify by column scan below */
        (void)cx; (void)startx;
        /* verify with a vertical scan at the candidate center column */
        int cc = x - (runlen) - 1; /* last x is one past; approximate */
        /* recompute candidate center x from the run widths */
        /* The 5 runs started at some sx; center of middle run = sx + w0+w1+w2/2 */
        /* We don't have sx; do a second horizontal pass to get exact center. */
        (void)cc;
        /* Second pass: find exact start by re-scanning this row's runs fully. */
        {
            int sx = -1; int r2[5], rc2 = 0, pv = -1, rl = 0;
            for (int xx = 0; xx < W; xx++) {
                int d = qr_is_dark(pix[y * W + xx], bg) ? 1 : 0;
                if (d != pv) { if (pv != -1) { r2[rc2++] = rl; if (rc2 == 5) { sx = xx - rl; break; } } pv = d; rl = 1; }
                else rl++;
            }
            if (sx < 0) continue;
            int mw = r2[2];
            int centerx = sx + r2[0] + r2[1] + r2[2] / 2;
            cands[nc].x = centerx; cands[nc].y = y; cands[nc].mw = mw; nc++;
        }
    }
    int found = 0;
    /* 2) cluster candidates into finders (centers within ~3 module-widths). */
    int *used = (int*)calloc((size_t)nc, sizeof(int));
    Cand *finders = (Cand*)malloc(sizeof(Cand) * (size_t)(nc + 1));
    int nf = 0;
    for (int i = 0; i < nc; i++) {
        if (used[i]) continue;
        int sx = 0, sy = 0, sn = 0, mwsum = 0;
        for (int j = i; j < nc; j++) {
            if (used[j]) continue;
            if (abs(cands[i].x - cands[j].x) <= 3 * cands[i].mw &&
                abs(cands[i].y - cands[j].y) <= 3 * cands[i].mw) {
                sx += cands[j].x; sy += cands[j].y; mwsum += cands[j].mw; sn++; used[j] = 1;
            }
        }
        finders[nf].x = sx / sn; finders[nf].y = sy / sn; finders[nf].mw = mwsum / sn; nf++;
    }
    /* 3) need at least 3 finders; pick the trio forming the symbol. For a clean
     *    scan the three finders are the corners (TL, TR, BL). Use the three most
     *    separated ones. */
    free(cands); free(used);
    if (nf < 3) { free(finders); return 0; }

    /* choose 3 finders: prefer the configuration with the largest bounding box
     * formed by 3 mutually-distant points. Simple: take global extremes. */
    int idx[3] = {0, 1, 2};
    /* sort by x then pick spread; for typical layout, the three finders are
     * enough; just use first three distinct if nf==3, else pick max-spread trio. */
    if (nf > 3) {
        /* pick the trio maximizing sum of pairwise distances */
        int best = -1;
        for (int a = 0; a < nf; a++) for (int b = a+1; b < nf; b++) for (int c = b+1; c < nf; c++) {
            int d = abs(finders[a].x-finders[b].x)+abs(finders[a].y-finders[b].y)
                   +abs(finders[b].x-finders[c].x)+abs(finders[b].y-finders[c].y)
                   +abs(finders[a].x-finders[c].x)+abs(finders[a].y-finders[c].y);
            if (d > best) { best = d; idx[0]=a; idx[1]=b; idx[2]=c; }
        }
    }
    /* map to TL, TR, BL by coordinates: TL=min(x+y), TR=max(x-y), BL=max(x+y) */
    Cand tri[3];
    for (int k = 0; k < 3; k++) tri[k] = finders[idx[k]];
    Cand *TL = &tri[0], *TR = &tri[1], *BL = &tri[2];
    /* sort by x+y ascending => TL first */
    for (int a = 0; a < 3; a++) for (int b = a+1; b < 3; b++) {
        int sa = tri[a].x + tri[a].y, sb = tri[b].x + tri[b].y;
        if (sa > sb) { Cand t = tri[a]; tri[a] = tri[b]; tri[b] = t; }
    }
    TL = &tri[0];
    /* among remaining two, TR has smaller y (top), BL has larger y */
    if (tri[1].y < tri[2].y) { TR = &tri[1]; BL = &tri[2]; }
    else { TR = &tri[2]; BL = &tri[1]; }

    /* 4) estimate module size and symbol dimension from finder spacing */
    double dTR = hypot((double)(TR->x - TL->x), (double)(TR->y - TL->y));
    double dBL = hypot((double)(BL->x - TL->x), (double)(BL->y - TL->y));
    /* module size ~ distance / (size-7) for v>=1; solve via version guess */
    int est_modules = 0;
    for (int vv = 1; vv <= 7; vv++) {
        int N = 17 + 4 * vv;
        double ms1 = dTR / (N - 7), ms2 = dBL / (N - 7);
        if (fabs(ms1 - ms2) / (ms1 + 1e-6) < 0.15) { est_modules = N; break; }
    }
    if (est_modules == 0) est_modules = 21;
    int N = est_modules;
    double ms = (dTR + dBL) / (2.0 * (N - 7));
    if (ms < 1) ms = 1;

    /* 5) affine map dst(module r,c)->src(pixel). Solve forward map src=A*dst. */
    /* dst TL=(0,0), TR=(N-1,0), BL=(0,N-1); src known. */
    double x0p = TL->x, y0p = TL->y;
    double x1p = TR->x, y1p = TR->y;
    double x2p = BL->x, y2p = BL->y;
    double a = (x1p - x0p) / (double)(N - 1);
    double b = (x2p - x0p) / (double)(N - 1);
    double c = (y1p - y0p) / (double)(N - 1);
    double d = (y2p - y0p) / (double)(N - 1);

    unsigned char *matrix = (unsigned char*)malloc((size_t)N * N);
    for (int r = 0; r < N; r++) for (int cc2 = 0; cc2 < N; cc2++) {
        double sx = x0p + a * cc2 + b * r;
        double sy = y0p + c * cc2 + d * r;
        int ix = (int)floor(sx + 0.5), iy = (int)floor(sy + 0.5);
        int v = 0;
        if (ix >= 0 && ix < W && iy >= 0 && iy < H) v = qr_is_dark(pix[iy * W + ix], bg) ? 1 : 0;
        matrix[r * N + cc2] = (unsigned char)v;
    }

    char dec[256];
    if (qr_decode_matrix(matrix, N, dec, sizeof dec) == 0 && dec[0]) {
        if (found < maxn) {
            strncpy(text[found], dec, 255); text[found][255] = 0;
            if (x0) {
                int minx = TL->x, miny = TL->y, maxx = TR->x, maxy = BL->y;
                x0[found] = minx - 3 * (int)ms; y0[found] = miny - 3 * (int)ms;
                x1[found] = maxx + 3 * (int)ms; y1[found] = maxy + 3 * (int)ms;
            }
            found++;
        }
    }
    free(matrix);
    free(finders);
    return found;
}

/* self-test: rs_encode then rs_decode, including error correction */
static int qr_rs_selftest(void){
    gf_init();
    int nroots = 10, ndata = 16, n = ndata + nroots;
    uint8_t data[16];
    for (int i = 0; i < 16; i++) data[i] = (uint8_t)(i * 7 + 3);
    int fail = 0;
    /* clean */
    uint8_t cw[26];
    for (int i = 0; i < 16; i++) cw[i] = data[i];
    rs_encode(cw, ndata, nroots, cw + ndata);
    if (rs_decode(cw, n, nroots) != 0) { fprintf(stderr, "SELFTEST clean FAIL\n"); fail = 1; }
    else fprintf(stderr, "SELFTEST clean OK\n");
    /* 1-error */
    uint8_t c1[26];
    for (int i = 0; i < n; i++) c1[i] = cw[i];
    c1[7] ^= 0x55;
    if (rs_decode(c1, n, nroots) != 0) { fprintf(stderr, "SELFTEST 1-err FAIL\n"); fail = 1; }
    else { int ok = 1; for (int i = 0; i < n; i++) if (c1[i] != cw[i]) ok = 0;
           fprintf(stderr, "SELFTEST 1-err %s\n", ok ? "OK" : "FAIL"); if (!ok) fail = 1; }
    /* 2-error */
    uint8_t c2[26];
    for (int i = 0; i < n; i++) c2[i] = cw[i];
    c2[3] ^= 0xAA; c2[11] ^= 0x33;
    if (rs_decode(c2, n, nroots) != 0) { fprintf(stderr, "SELFTEST 2-err FAIL\n"); fail = 1; }
    else { int ok = 1; for (int i = 0; i < n; i++) if (c2[i] != cw[i]) ok = 0;
           fprintf(stderr, "SELFTEST 2-err %s\n", ok ? "OK" : "FAIL"); if (!ok) fail = 1; }
    return fail ? -1 : 0;
}

