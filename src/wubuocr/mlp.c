/* mlp.c -- lightweight feed-forward classifier. See mlp.h. C11, no deps. */
#include "mlp.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

struct MLP {
    int din, h1, h2, K;
    float *W1, *b1, *W2, *b2, *W3, *b3;
    /* cached activations (last forward) for gradient computation */
    float *h1act, *h2act;
    /* gradient buffers (kept opaque; updated by mlp_train_step) */
    float *gW1, *gb1, *gW2, *gb2, *gW3, *gb3;
    /* input gradient dL/dz for the last backward() call (feeds an upstream
     * module such as ConvNet) */
    float *dz;
    int shared_params;   /* 1 if weight pointers alias another MLP (grad-only replica) */
};

/* Small, deterministic xorshift RNG for He-init (module-local, no deps). */
static uint32_t s_rng;
static void   s_seed(uint32_t s) { s_rng = s ? s : 0x9E3779B9u; }
static float  s_uni(float lo, float hi) {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    float u = (float)s_rng / (float)0xFFFFFFFFu;
    return lo + u * (hi - lo);
}

MLP *mlp_create(int din, int h1, int h2, int K, uint32_t seed) {
    MLP *m = (MLP *)malloc(sizeof(*m));
    if (!m) return NULL;
    m->din = din; m->h1 = h1; m->h2 = h2; m->K = K;
    m->W1 = (float *)calloc((size_t)din * h1, sizeof(float));
    m->b1 = (float *)calloc((size_t)h1, sizeof(float));
    m->W2 = (float *)calloc((size_t)h1 * h2, sizeof(float));
    m->b2 = (float *)calloc((size_t)h2, sizeof(float));
    m->W3 = (float *)calloc((size_t)h2 * K, sizeof(float));
    m->b3 = (float *)calloc((size_t)K, sizeof(float));
    m->h1act = (float *)malloc((size_t)h1 * sizeof(float));
    m->h2act = (float *)malloc((size_t)h2 * sizeof(float));
    m->gW1 = (float *)calloc((size_t)din * h1, sizeof(float));
    m->gb1 = (float *)calloc((size_t)h1, sizeof(float));
    m->gW2 = (float *)calloc((size_t)h1 * h2, sizeof(float));
    m->gb2 = (float *)calloc((size_t)h2, sizeof(float));
    m->gW3 = (float *)calloc((size_t)h2 * K, sizeof(float));
    m->gb3 = (float *)calloc((size_t)K, sizeof(float));
    m->dz  = (float *)calloc((size_t)din, sizeof(float));
    if (!m->W1 || !m->b1 || !m->W2 || !m->b2 || !m->W3 || !m->b3 ||
        !m->h1act || !m->h2act || !m->gW1 || !m->gb1 || !m->gW2 || !m->gb2 ||
        !m->gW3 || !m->gb3 || !m->dz) { mlp_destroy(m); return NULL; }

    s_seed(seed ? seed : 0x1234ABCDu);
    float s1 = sqrtf(2.0f / (float)din);
    float s2 = sqrtf(2.0f / (float)h1);
    float s3 = sqrtf(2.0f / (float)h2);
    for (int i = 0; i < din * h1; i++) m->W1[i] = s_uni(-1, 1) * s1;
    for (int i = 0; i < h1 * h2; i++) m->W2[i] = s_uni(-1, 1) * s2;
    for (int i = 0; i < h2 * K;  i++) m->W3[i] = s_uni(-1, 1) * s3;
    m->shared_params = 0;
    return m;
}

/* Grad-buffer-only replica: caches + grads allocated, weight pointers ALIASED
 * to src (read-only during a batch). Avoids copying weights every batch. */
MLP *mlp_gradbuf(const MLP *s) {
    if (!s) return NULL;
    MLP *m = (MLP *)malloc(sizeof(*m));
    if (!m) return NULL;
    *m = *s;
    m->W1=(float*)s->W1; m->b1=(float*)s->b1;
    m->W2=(float*)s->W2; m->b2=(float*)s->b2;
    m->W3=(float*)s->W3; m->b3=(float*)s->b3;
    m->shared_params = 1;
    m->h1act = malloc((size_t)s->h1*sizeof(float));
    m->h2act = malloc((size_t)s->h2*sizeof(float));
    m->gW1 = calloc((size_t)s->din*s->h1,sizeof(float));
    m->gb1 = calloc((size_t)s->h1,sizeof(float));
    m->gW2 = calloc((size_t)s->h1*s->h2,sizeof(float));
    m->gb2 = calloc((size_t)s->h2,sizeof(float));
    m->gW3 = calloc((size_t)s->h2*s->K,sizeof(float));
    m->gb3 = calloc((size_t)s->K,sizeof(float));
    m->dz  = calloc((size_t)s->din,sizeof(float));
    return m;
}

void mlp_destroy(MLP *m) {
    if (!m) return;
    if (!m->shared_params) {
        free(m->W1); free(m->b1); free(m->W2); free(m->b2);
        free(m->W3); free(m->b3);
    }
    free(m->h1act); free(m->h2act);
    free(m->gW1); free(m->gb1); free(m->gW2); free(m->gb2);
    free(m->gW3); free(m->gb3); free(m->dz);
    free(m);
}

int mlp_din(const MLP *m) { return m->din; }
int mlp_h1(const MLP *m)  { return m->h1; }
int mlp_h2(const MLP *m)  { return m->h2; }
int mlp_K(const MLP *m)   { return m->K; }

void mlp_forward(const MLP *m, const float *z, float *out_scores) {
    float *h1 = m->h1act, *h2 = m->h2act;
    for (int j = 0; j < m->h1; j++) {
        float s = m->b1[j];
        const float *Wr = m->W1 + (size_t)j * m->din;
        for (int i = 0; i < m->din; i++) s += Wr[i] * z[i];
        h1[j] = s > 0 ? s : MLP_LEAK * s;
    }
    for (int j = 0; j < m->h2; j++) {
        float s = m->b2[j];
        const float *Wr = m->W2 + (size_t)j * m->h1;
        for (int i = 0; i < m->h1; i++) s += Wr[i] * h1[i];
        h2[j] = s > 0 ? s : MLP_LEAK * s;
    }
    for (int c = 0; c < m->K; c++) {
        float s = m->b3[c];
        const float *Wr = m->W3 + (size_t)c * m->h2;
        for (int i = 0; i < m->h2; i++) s += Wr[i] * h2[i];
        out_scores[c] = s;
    }
}

void mlp_zero_grad(MLP *m) {
    memset(m->gW1, 0, (size_t)m->din * m->h1 * sizeof(float));
    memset(m->gb1, 0, (size_t)m->h1 * sizeof(float));
    memset(m->gW2, 0, (size_t)m->h1 * m->h2 * sizeof(float));
    memset(m->gb2, 0, (size_t)m->h2 * sizeof(float));
    memset(m->gW3, 0, (size_t)m->h2 * m->K * sizeof(float));
    memset(m->gb3, 0, (size_t)m->K * sizeof(float));
}

void mlp_backward(MLP *m, const float *z, int target) {
    float *h1 = m->h1act, *h2 = m->h2act;

    /* softmax of cached scores (recompute from stored activations) */
    float scores[MLP_NCLASS_MAX], sm[MLP_NCLASS_MAX];
    for (int c = 0; c < m->K; c++) {
        float s = m->b3[c];
        const float *Wr = m->W3 + (size_t)c * m->h2;
        for (int i = 0; i < m->h2; i++) s += Wr[i] * h2[i];
        scores[c] = s;
    }
    float mx = scores[0];
    for (int c = 1; c < m->K; c++) if (scores[c] > mx) mx = scores[c];
    float sum = 0;
    for (int c = 0; c < m->K; c++) { sm[c] = expf(scores[c] - mx); sum += sm[c]; }
    for (int c = 0; c < m->K; c++) sm[c] /= sum;

    /* output grad: dscore[c] = softmax[c] - onehot[c] */
    float dsc[MLP_NCLASS_MAX];
    for (int c = 0; c < m->K; c++) dsc[c] = sm[c] - (c == target ? 1.0f : 0.0f);

    /* W3 += dsc (outer) h2 ; b3 += dsc */
    for (int c = 0; c < m->K; c++) {
        float *gW3c = m->gW3 + (size_t)c * m->h2;
        for (int i = 0; i < m->h2; i++) gW3c[i] += dsc[c] * h2[i];
        m->gb3[c] += dsc[c];
    }

    /* hidden2 grad: dh2[i] = (sum_c dsc[c]*W3[c,i]) * leak-or-1 */
    float *dh2 = (float *)malloc((size_t)m->h2 * sizeof(float));
    for (int i = 0; i < m->h2; i++) {
        float g = 0;
        for (int c = 0; c < m->K; c++) g += dsc[c] * m->W3[(size_t)c * m->h2 + i];
        dh2[i] = (h2[i] > 0 ? g : MLP_LEAK * g);
    }
    for (int j = 0; j < m->h2; j++) {
        float *gW2j = m->gW2 + (size_t)j * m->h1;
        for (int i = 0; i < m->h1; i++) gW2j[i] += dh2[j] * h1[i];
        m->gb2[j] += dh2[j];
    }

    /* hidden1 grad: dh1[i] = (sum_j dh2[j]*W2[j,i]) * leak-or-1 */
    float *dh1 = (float *)malloc((size_t)m->h1 * sizeof(float));
    for (int i = 0; i < m->h1; i++) {
        float g = 0;
        for (int j = 0; j < m->h2; j++) g += dh2[j] * m->W2[(size_t)j * m->h1 + i];
        dh1[i] = (h1[i] > 0 ? g : MLP_LEAK * g);
    }
    for (int j = 0; j < m->h1; j++) {
        float *gW1j = m->gW1 + (size_t)j * m->din;
        for (int i = 0; i < m->din; i++) gW1j[i] += dh1[j] * z[i];
        m->gb1[j] += dh1[j];
    }
    /* dL/dz[i] = sum_j dL/dh1[j] * W1[j,i]  (for upstream backprop) */
    for (int i = 0; i < m->din; i++) {
        float g = 0;
        for (int j = 0; j < m->h1; j++) g += dh1[j] * m->W1[(size_t)j * m->din + i];
        m->dz[i] = g;
    }
    free(dh2); free(dh1);
}

/* legacy single-sample step (kept for small tests / back-compat) */
void mlp_train_step(MLP *m, const float *z, int target) {
    mlp_zero_grad(m);
    mlp_backward(m, z, target);
}

void mlp_input_grad(MLP *m, const float *z, float *out_dz) {
    (void)z;   /* gradient already stored in m->dz by the last mlp_backward */
    for (int i = 0; i < m->din; i++) out_dz[i] = m->dz[i];
}

void mlp_scale_grad(MLP *m, float s) {
    for (int i = 0; i < m->din * m->h1; i++) m->gW1[i] *= s;
    for (int i = 0; i < m->h1; i++)        m->gb1[i] *= s;
    for (int i = 0; i < m->h1 * m->h2; i++) m->gW2[i] *= s;
    for (int i = 0; i < m->h2; i++)       m->gb2[i] *= s;
    for (int i = 0; i < m->h2 * m->K; i++)  m->gW3[i] *= s;
    for (int i = 0; i < m->K; i++)         m->gb3[i] *= s;
}
/* Accumulate src gradient buffers into dst (thread-private -> shared reduce). */
void mlp_add_grad(MLP *dst, const MLP *src) {
    for (int i = 0; i < dst->din * dst->h1; i++) dst->gW1[i] += src->gW1[i];
    for (int i = 0; i < dst->h1; i++)        dst->gb1[i] += src->gb1[i];
    for (int i = 0; i < dst->h1 * dst->h2; i++) dst->gW2[i] += src->gW2[i];
    for (int i = 0; i < dst->h2; i++)       dst->gb2[i] += src->gb2[i];
    for (int i = 0; i < dst->h2 * dst->K; i++)  dst->gW3[i] += src->gW3[i];
    for (int i = 0; i < dst->K; i++)         dst->gb3[i] += src->gb3[i];
}

int mlp_layer_count(const MLP *m) { (void)m; return 6; }

MLPLayer mlp_layer(MLP *m, int idx) {
    MLPLayer L;
    switch (idx) {
        case 0: L.param = m->W1; L.grad = m->gW1; L.n = m->din * m->h1; break;
        case 1: L.param = m->b1; L.grad = m->gb1; L.n = m->h1;        break;
        case 2: L.param = m->W2; L.grad = m->gW2; L.n = m->h1 * m->h2; break;
        case 3: L.param = m->b2; L.grad = m->gb2; L.n = m->h2;        break;
        case 4: L.param = m->W3; L.grad = m->gW3; L.n = m->h2 * m->K;  break;
        default:L.param = m->b3; L.grad = m->gb3; L.n = m->K;         break;
    }
    return L;
}

void mlp_apply_plain(MLP *m, float lr) {
    for (int g = 0; g < 6; g++) {
        MLPLayer L = mlp_layer(m, g);
        for (int i = 0; i < L.n; i++) L.param[i] -= lr * L.grad[i];
    }
}

/* ---- save / load ---- */
int mlp_save(const MLP *m, const float *zmean, const float *zstd, int dim,
             const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "wubu_ocr_mlp_v1 %d %d %d %d %d\n",
            m->din, m->h1, m->h2, m->K, dim);
    for (int d = 0; d < dim; d++) fprintf(f, "%a\n", zmean[d]);
    for (int d = 0; d < dim; d++) fprintf(f, "%a\n", zstd[d]);
    #define DUMP(arr, n) for (int i = 0; i < (n); i++) fprintf(f, "%a\n", (arr)[i]);
    DUMP(m->W1, m->din * m->h1) DUMP(m->b1, m->h1)
    DUMP(m->W2, m->h1 * m->h2)  DUMP(m->b2, m->h2)
    DUMP(m->W3, m->h2 * m->K)   DUMP(m->b3, m->K)
    #undef DUMP
    fclose(f);
    return 0;
}

int mlp_load(const char *path, MLP **out,
             float *zmean, float *zstd, int *dim) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int din, h1, h2, K, d2;
    if (fscanf(f, "wubu_ocr_mlp_v1 %d %d %d %d %d\n", &din, &h1, &h2, &K, &d2) != 5) {
        fclose(f); return -1;
    }
    MLP *m = mlp_create(din, h1, h2, K, 1);
    if (!m) { fclose(f); return -1; }
    float *zm = zmean ? zmean : malloc((size_t)d2*sizeof(float));
    float *zs = zstd  ? zstd  : malloc((size_t)d2*sizeof(float));
    for (int i = 0; i < d2; i++) if (fscanf(f, "%a\n", &zm[i]) != 1) { mlp_destroy(m); fclose(f); return -1; }
    for (int i = 0; i < d2; i++) if (fscanf(f, "%a\n", &zs[i]) != 1)  { mlp_destroy(m); fclose(f); return -1; }
    if (!zmean) free(zm);
    if (!zstd)  free(zs);
    #define READ(arr, n) for (int i = 0; i < (n); i++) if (fscanf(f, "%a\n", &(arr)[i]) != 1) { mlp_destroy(m); fclose(f); return -1; }
    READ(m->W1, din * h1) READ(m->b1, h1)
    READ(m->W2, h1 * h2)  READ(m->b2, h2)
    READ(m->W3, h2 * K)   READ(m->b3, K)
    #undef READ
    fclose(f);
    *out = m; *dim = d2;
    return 0;
}
