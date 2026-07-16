/* emnist_train_wubu.c -- "Steve Jobs" C11 OCR: principled zoning + wubu math.
 *
 * Principle (kept): glyph -> zoning feature vector z (interpretable, 90s-tech,
 * no neural net for feature extraction).
 *
 * Excellence (added): instead of a hand-set 1-NN distance, we LEARN the metric.
 * A linear discriminant  s_c = W_c . z + b_c  (W is K x dim, K=26 classes) is
 * trained on EMNIST Letters with the wubu Riemannian SGD optimizer and the wubu
 * Q-Controller adaptive learning rate. Recognition is still argmax over the
 * learned prototypes -- our 1-NN-in-learned-metric principle, now optimal.
 *
 * Pure C11, zero external deps. Uses WuBuMath (../WuBuMath) for the optimizer.
 *
 * Build: see tools/build_wubu_ocr.sh
 * Usage: emnist_train_wubu <data/emnist> [grid] [epochs] [traincap] [testcap]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wubumath.h"

#define EMNIST_NCLASS 26
#define IDX_MAGIC_IMAGE 0x00000803
#define IDX_MAGIC_LABEL 0x00000801

/* ---- IDX loading (same conventions as emnist_eval.c) ---- */
static long idx_count(const unsigned char *hdr) {
    return ((long)hdr[4] << 24) | ((long)hdr[5] << 16) |
           ((long)hdr[6] << 8)  | (long)hdr[7];
}
static int load_idx(const char *path, unsigned char **data, long *count,
                    int *rows, int *cols) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    unsigned char hdr[16];
    if (fread(hdr, 1, 16, f) != 16) { fclose(f); return -1; }
    long n = idx_count(hdr);
    int r = 1, c = 1;
    if (hdr[3] == 3) {                 /* 3-D image tensor: dims are 4-byte BE */
        r = ((int)hdr[8]<<24)|((int)hdr[9]<<16)|((int)hdr[10]<<8)|hdr[11];
        c = ((int)hdr[12]<<24)|((int)hdr[13]<<16)|((int)hdr[14]<<8)|hdr[15];
    }                                   /* 1-D label vector: pic = 1 */
    int pic = r * c;
    if (pic < 1) pic = 1;
    unsigned char *buf = malloc((size_t)n * pic);
    if (!buf) { fclose(f); return -1; }
    size_t want = (size_t)n * pic;
    size_t got = fread(buf, 1, want, f);
    /* Tolerate minor truncation (e.g. gunzip artifacts): report actual count. */
    *count = (long)(got / pic);
    *data = buf;
    fclose(f);
}
/* ---- Principled zoning feature (kept from the C11 OCR core) ----
 * Zone a raw grayscale glyph (row-major, 0=bg .. 255=ink) into an NxN grid,
 * using the tight ink bounding box (scale/translation invariant). ink = dark. */
static int count_holes(const unsigned char *px, int w, int h) {
    int n = w * h;
    unsigned char *bg = malloc(n > 0 ? n : 1);
    if (!bg) return 0;
    for (int i = 0; i < n; i++) bg[i] = (px[i] <= 127) ? 0 : 1; /* 1 = bg */
    int *stk = malloc((n > 0 ? n : 1) * sizeof(int));
    if (!stk) { free(bg); return 0; }
    int sp = 0;
    for (int x = 0; x < w; x++) {
        if (bg[x]) stk[sp++] = x;
        if (bg[(h-1)*w + x]) stk[sp++] = (h-1)*w + x;
    }
    for (int y = 0; y < h; y++) {
        if (bg[y*w]) stk[sp++] = y*w;
        if (bg[y*w + w-1]) stk[sp++] = y*w + w-1;
    }
    while (sp) {
        int p = stk[--sp]; int cx = p % w, cy = p / w;
        int nb[4][2] = {{cx-1,cy},{cx+1,cy},{cx,cy-1},{cx,cy+1}};
        for (int k = 0; k < 4; k++) {
            int nx = nb[k][0], ny = nb[k][1];
            if (nx<0||ny<0||nx>=w||ny>=h) continue;
            int q = ny*w+nx;
            if (bg[q]) { bg[q]=0; stk[sp++]=q; }
        }
    }
    int holes = 0;
    for (int i = 0; i < n; i++) {
        if (!bg[i]) continue;
        holes++;
        sp = 0; stk[sp++] = i; bg[i] = 0;
        while (sp) {
            int p = stk[--sp]; int cx = p % w, cy = p / w;
            int nb[4][2] = {{cx-1,cy},{cx+1,cy},{cx,cy-1},{cx,cy+1}};
            for (int k = 0; k < 4; k++) {
                int nx = nb[k][0], ny = nb[k][1];
                if (nx<0||ny<0||nx>=w||ny>=h) continue;
                int q = ny*w+nx;
                if (bg[q]) { bg[q]=0; stk[sp++]=q; }
            }
        }
    }
    free(stk); free(bg);
    return holes;
}

/* Compute zoning vector z[grid*grid] (each cell = ink fraction in [0,1]) plus
 * two structural features appended: aspect ratio and hole count. Returns the
 * feature dimension (grid*grid + 2). */
static int zoning_features(const unsigned char *px, int w, int h, int grid,
                           float *z) {
    int minx = w, miny = h, maxx = 0, maxy = 0, found = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (px[y*w+x] <= 127) {
                found = 1;
                if (x<minx) minx=x; if (y<miny) miny=y;
                if (x>maxx) maxx=x; if (y>maxy) maxy=y;
            }
    int dim = grid*grid;
    for (int i = 0; i < dim+2; i++) z[i] = 0.0f;
    if (!found) return dim + 2;
    int bw = maxx-minx+1, bh = maxy-miny+1;
    int cw = (bw + grid - 1) / grid, ch = (bh + grid - 1) / grid;
    for (int gy = 0; gy < grid; gy++)
        for (int gx = 0; gx < grid; gx++) {
            int cnt = 0, ink = 0;
            int x0 = minx + gx*cw, y0 = miny + gy*ch;
            for (int y = y0; y < y0+ch && y <= maxy; y++)
                for (int x = x0; x < x0+cw && x <= maxx; x++)
                    if (y>=0&&y<h&&x>=0&&x<w) {
                        cnt++;
                        if (px[y*w+x] <= 127) ink++;
                    }
            z[gy*grid + gx] = cnt ? (float)ink/(float)cnt : 0.0f;
        }
    z[dim]   = (bh>0) ? (float)bw/(float)bh : 1.0f;   /* aspect ratio */
    z[dim+1] = (float)count_holes(px, w, h);            /* hole count   */
    return dim + 2;
}

/* ---- Learned linear discriminant model: s_c = W_c . z + b_c ---- */
typedef struct {
    int dim;                 /* feature dim (grid*grid + 2) */
    int K;                   /* number of classes */
    float *W;                /* [K * dim] */
    float *b;                /* [K] */
} LinModel;

static void model_init(LinModel *m, int dim, int K, WubuRNG *rng) {
    m->dim = dim; m->K = K;
    m->W = calloc((size_t)K * dim, sizeof(float));
    m->b = calloc(K, sizeof(float));
    /* small random init (Xavier-ish) for symmetry breaking */
    float scale = 0.1f / sqrtf((float)dim);
    for (int i = 0; i < K*dim; i++)
        m->W[i] = (wubu_rng_uniform(rng, -1.0f, 1.0f)) * scale;
}

static void model_free(LinModel *m) {
    free(m->W); free(m->b); m->W = NULL; m->b = NULL;
}

/* forward: scores[K] = W.z + b ; also fills softmax[K] */
static void model_forward(const LinModel *m, const float *z,
                          float *scores, float *softmax) {
    for (int c = 0; c < m->K; c++) {
        float s = m->b[c];
        const float *Wc = m->W + (size_t)c * m->dim;
        for (int i = 0; i < m->dim; i++) s += Wc[i] * z[i];
        scores[c] = s;
    }
    /* softmax (numerically stable) */
    float mx = scores[0];
    for (int c = 1; c < m->K; c++) if (scores[c] > mx) mx = scores[c];
    float sum = 0.0f;
    for (int c = 0; c < m->K; c++) { softmax[c] = expf(scores[c] - mx); sum += softmax[c]; }
    for (int c = 0; c < m->K; c++) softmax[c] /= sum;
}

/* One SGD step on a (z, target) pair using wubu Riemannian SGD (euclidean).
 * grad: dW[c][i] = (softmax[c] - onehot[c]) * z[i] ; db[c] = (softmax[c]-onehot) */
static void model_step(LinModel *m, const float *z, int target,
                       WubuSGD *optW, WubuSGD *optB) {
    float scores[EMNIST_NCLASS], sm[EMNIST_NCLASS];
    model_forward(m, z, scores, sm);
    float gW[EMNIST_NCLASS * (64*64 + 2)];  /* max dim 64*64+2 */
    float gB[EMNIST_NCLASS];
    for (int c = 0; c < m->K; c++) {
        float e = sm[c] - (c == target ? 1.0f : 0.0f);
        gB[c] = e;
        const float *Wc = m->W + (size_t)c * m->dim;
        float *gWc = gW + (size_t)c * m->dim;
        for (int i = 0; i < m->dim; i++) gWc[i] = e * z[i];
        (void)Wc;
    }
    wubu_sgd_step_euclidean(optW, m->W, gW, m->K * m->dim);
    wubu_sgd_step_euclidean(optB, m->b, gB, m->K);
}

/* ---- weights save / load (plain text, git-ignored corpus artifact) ---- */
static int model_save(const LinModel *m, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "wubu_ocr_v1 %d %d\n", m->dim, m->K);
    for (int i = 0; i < m->K*m->dim; i++) fprintf(f, "%a\n", m->W[i]);
    for (int i = 0; i < m->K; i++) fprintf(f, "%a\n", m->b[i]);
    fclose(f);
    return 0;
}
static int model_load(LinModel *m, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char tag[32]; int dim, K;
    if (fscanf(f, "%31s %d %d", tag, &dim, &K) != 3) { fclose(f); return -1; }
    if (strcmp(tag, "wubu_ocr_v1") != 0) { fclose(f); return -1; }
    m->dim = dim; m->K = K;
    m->W = calloc((size_t)K*dim, sizeof(float));
    m->b = calloc(K, sizeof(float));
    for (int i = 0; i < K*dim; i++) if (fscanf(f, "%a", &m->W[i]) != 1) { fclose(f); return -1; }
    for (int i = 0; i < K; i++)     if (fscanf(f, "%a", &m->b[i]) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

/* ---- main ---- */
int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : "data/emnist";
    int grid   = (argc > 2) ? atoi(argv[2]) : 8;
    int epochs = (argc > 3) ? atoi(argv[3]) : 12;
    long traincap = (argc > 4) ? atol(argv[4]) : 0;   /* 0 = all */
    long testcap  = (argc > 5) ? atol(argv[5]) : 0;

    char ptr[512], pte[512], ptw[512];
    snprintf(ptr, sizeof ptr, "%s/emnist-letters-train-images-idx3-ubyte", dir);
    snprintf(pte, sizeof pte, "%s/emnist-letters-test-images-idx3-ubyte", dir);
    snprintf(ptw, sizeof ptw, "%s/emnist-letters-train-labels-idx1-ubyte", dir);

    unsigned char *tr_img = NULL, *te_img = NULL, *tr_lab = NULL, *te_lab = NULL;
    long ntr = 0, nte = 0, ntw = 0, nte_l = 0; int rows = 0, cols = 0;
    if (load_idx(ptr, &tr_img, &ntr, &rows, &cols)) return 1;
    if (load_idx(pte, &te_img, &nte, &rows, &cols)) return 1;
    if (load_idx(ptw, &tr_lab, &ntw, &rows, &cols)) return 1;
    (void)ntw;
    char ptel[512];
    snprintf(ptel, sizeof ptel, "%s/emnist-letters-test-labels-idx1-ubyte", dir);
    if (load_idx(ptel, &te_lab, &nte_l, &rows, &cols)) return 1;
    if (traincap && traincap < ntr) ntr = traincap;
    if (testcap  && testcap  < nte) nte = testcap;
    if (ntr > ntw) ntr = ntw;          /* clamp to actual label count */
    if (nte > nte_l) nte = nte_l;

    int dim = grid*grid + 2;
    long featsz = (size_t)ntr * dim;
    float *Z = malloc(featsz * sizeof(float));   /* cached train features */
    if (!Z) { fprintf(stderr, "oom features\n"); return 1; }

    /* Precompute zoning features for the (capped) train set. EMNIST ink is
     * HIGH value; invert to dark-ink convention used by zoning_features. */
    printf("computing zoning features for %ld train glyphs (grid=%d, dim=%d)...\n",
           ntr, grid, dim);
    for (long i = 0; i < ntr; i++) {
        unsigned char inv[784];
        const unsigned char *raw = tr_img + i*784;
        for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
        zoning_features(inv, 28, 28, grid, Z + (size_t)i*dim);
    }

    /* model + optimizers + Q-controller */
    WubuRNG rng; wubu_rng_init(&rng, 0x1234ABCDu);
    LinModel m; model_init(&m, dim, EMNIST_NCLASS, &rng);

    WubuSGDConfig cfgW = {0}, cfgB = {0};
    cfgW.learning_rate = 0.05f; cfgW.momentum_factor = 0.9f;
    cfgW.weight_decay = 1e-4f;  cfgW.max_grad_norm = 5.0f;
    cfgW.q_controller_enabled = 1;
    cfgB = cfgW;
    WubuManifoldBinding man = {0}; man.manifold_enabled = 0;
    WubuSGD optW, optB;
    wubu_sgd_init(&optW, &cfgW, man, m.K*m.dim);
    wubu_sgd_init(&optB, &cfgB, man, m.K);

    QControllerConfig qcfg = WUBU_Q_CONTROLLER_DEFAULT;
    qcfg.lr_min = 1e-3f; qcfg.lr_max = 0.3f; qcfg.warmup_lr_start = 0.02f;
    QController qc; wubu_q_controller_init(&qc, &qcfg);

    long batch = 256;
    long nbatch = (ntr + batch - 1) / batch;
    printf("training %d epochs, %ld batches/epoch, lr via wubu Q-Controller...\n",
           epochs, nbatch);

    for (int ep = 0; ep < epochs; ep++) {
        long correct = 0;
        /* shuffle indices */
        long *idx = malloc((size_t)ntr * sizeof(long));
        for (long i = 0; i < ntr; i++) idx[i] = i;
        for (long i = ntr-1; i > 0; i--) {
            long j = (long)(wubu_rng_next(&rng) % (unsigned long)(i+1));
            long t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
        float lr_state = optW.current_lr;
        for (long b = 0; b < nbatch; b++) {
            for (long k = 0; k < batch && b*batch+k < ntr; k++) {
                long s = idx[b*batch + k];
                int lab = tr_lab[s] - 1;
                if (lab < 0 || lab >= EMNIST_NCLASS) continue;
                model_step(&m, Z + (size_t)s*dim, lab, &optW, &optB);
                /* track training accuracy occasionally */
            }
            (void)lr_state;
        }
        /* adaptive LR via Q-controller using training-set accuracy proxy:
         * evaluate full train accuracy cheaply every epoch (subset). */
        long chk = ntr < 4000 ? ntr : 4000;
        for (long i = 0; i < chk; i++) {
            float sc[EMNIST_NCLASS], sm[EMNIST_NCLASS];
            model_forward(&m, Z + (size_t)i*dim, sc, sm);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c]>sc[best]) best=c;
            if (best == tr_lab[i]-1) correct++;
        }
        float acc = (float)correct/(float)chk;
        /* Optional fixed LR (bypass Q-Controller) via env WUBUIX_LR */
        char *envlr = getenv("WUBUIX_LR");
        if (envlr) {
            float flr = (float)atof(envlr);
            wubu_sgd_set_lr(&optW, flr);
            wubu_sgd_set_lr(&optB, flr);
            qc.current_lr = flr;
        } else {
            wubu_q_controller_update(&qc, acc, &qcfg);
            wubu_q_controller_choose_action(&qc, &rng, &qcfg, optW.current_lr);
            wubu_sgd_set_lr(&optW, qc.current_lr);
            wubu_sgd_set_lr(&optB, qc.current_lr);
        }
        float wnorm = 0.0f; for (int q = 0; q < m.K*m.dim; q++) wnorm += m.W[q]*m.W[q];
        printf("  epoch %2d: train_acc~%.2f%%  lr=%.4f  |W|=%.3f\n",
               ep+1, 100.0f*acc, qc.current_lr, sqrtf(wnorm));
        free(idx);
    }
    wubu_q_controller_free(&qc);
    wubu_sgd_free(&optW); wubu_sgd_free(&optB);

    /* ---- evaluate on test set ---- */
    long correct = 0;
    long cls_correct[EMNIST_NCLASS] = {0}, cls_total[EMNIST_NCLASS] = {0};
    unsigned char inv[784];
    for (long i = 0; i < nte; i++) {
        const unsigned char *raw = te_img + i*784;
        for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
        float z[64*64 + 2];
        zoning_features(inv, 28, 28, grid, z);
        float sc[EMNIST_NCLASS], sm[EMNIST_NCLASS];
        model_forward(&m, z, sc, sm);
        int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
        int truth = te_lab ? te_lab[i]-1 : -1;
        if (truth >= 0) { cls_total[truth]++; if (best == truth) { correct++; cls_correct[truth]++; } }
    }
    printf("\n=== EMNIST Letters (wubu-trained linear metric) ===\n");
    printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n",
           100.0f*(float)correct/(float)nte, correct, nte);

    /* save weights */
    char wpath[576];
    snprintf(wpath, sizeof wpath, "%s/emnist_wubu_g%d.wts", dir, grid);
    if (model_save(&m, wpath) == 0)
        printf("saved weights -> %s\n", wpath);

    model_free(&m);
    free(Z); free(tr_img); free(te_img); free(tr_lab); free(te_lab);
    return 0;
}

