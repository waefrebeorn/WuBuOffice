/* emnist_train.c -- train the lightweight wubu OCR MLP on EMNIST Letters.
 *
 * Pipeline: load IDX corpus -> extract zoning features (opaque ZoningExtractor)
 * -> standardize features (per-dim mean/std, std-guarded) -> train an MLP
 * classifier with the wubu Riemannian SGD optimizer (or plain SGD via
 * WUBUIX_PLAIN). Saves weights + standardization stats for inference.
 *
 * Build (standalone): see tools/build_wubu_ocr.sh
 * Usage: emnist_train <data/emnist> [grid] [h1] [h2] [epochs] [traincap] [testcap]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "zoning.h"
#include "mlp.h"
#include "wubumath.h"

#define IDX_MAGIC_IMAGE 0x00000803
#define EMNIST_NCLASS 26
#define MAXFEAT (64*64 + 8)

/* ---- IDX loading ---- */
static long idx_count(const unsigned char *hdr) {
    return ((long)hdr[4] << 24) | ((long)hdr[5] << 16) |
           ((long)hdr[6] << 8)  | (long)hdr[7];
}
/* Reads a single IDX file. For 3-D tensors (magic 0x0803) the element size is
 * rows*cols; for 1-D label tensors (magic 0x0801) it is 1 byte. */
static int load_idx(const char *path, unsigned char **data, long *count) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    unsigned char hdr[16];
    if (fread(hdr, 1, 16, f) != 16) { fclose(f); return -1; }
    long n = idx_count(hdr);
    int pic = 1;
    if (hdr[3] == 3) {   /* images: 3-D tensor */
        int r = ((int)hdr[8]<<24)|((int)hdr[9]<<16)|((int)hdr[10]<<8)|hdr[11];
        int c = ((int)hdr[12]<<24)|((int)hdr[13]<<16)|((int)hdr[14]<<8)|hdr[15];
        pic = r * c; if (pic < 1) pic = 1;
    }
    unsigned char *buf = malloc((size_t)n * pic);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)n * pic, f);
    *count = (long)(got / pic);
    *data = buf; fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);   /* unbuffered: live progress to logs */
    const char *dir = (argc > 1) ? argv[1] : "data/emnist";
    int grid   = (argc > 2) ? atoi(argv[2]) : 12;
    int h1     = (argc > 3) ? atoi(argv[3]) : 128;
    int h2     = (argc > 4) ? atoi(argv[4]) : 64;
    int epochs = (argc > 5) ? atoi(argv[5]) : 60;
    long traincap = (argc > 6) ? atol(argv[6]) : 0;
    (void)(argc > 7 ? atol(argv[7]) : 0L);  /* optional testcap reserved */

    char ptr[512], pte[512], ptw[512], ptel[512];
    snprintf(ptr, sizeof ptr, "%s/emnist-letters-train-images-idx3-ubyte", dir);
    snprintf(pte, sizeof pte, "%s/emnist-letters-test-images-idx3-ubyte", dir);
    snprintf(ptw, sizeof ptw, "%s/emnist-letters-train-labels-idx1-ubyte", dir);
    snprintf(ptel, sizeof ptel, "%s/emnist-letters-test-labels-idx1-ubyte", dir);

    unsigned char *tr_img = NULL, *te_img = NULL, *tr_lab = NULL, *te_lab = NULL;
    long ntr = 0, nte = 0, ntw = 0, nte_l = 0;
    if (load_idx(ptr, &tr_img, &ntr)) return 1;
    if (load_idx(pte, &te_img, &nte)) return 1;
    if (load_idx(ptw, &tr_lab, &ntw)) return 1;
    if (load_idx(ptel, &te_lab, &nte_l)) return 1;
    if (traincap && traincap < ntr) ntr = traincap;
    if (ntr > ntw) ntr = ntw;
    nte = nte_l;   /* full, balanced test split */

    ZoningExtractor *zx = zoning_create(grid);
    int use_raw = getenv("WUBUIX_RAW") ? 1 : 0;
    int dim = use_raw ? 784 : zoning_dim(zx);
    printf("feature dim=%d (%s); MLP %d->%d->%d->%d; epochs=%d; train=%ld test=%ld\n",
           dim, use_raw ? "raw 28x28" : "zoning", dim, h1, h2, EMNIST_NCLASS, epochs, ntr, nte);

    /* Precompute train features. RAW = inverted 28x28 pixels (ink DARK);
     * otherwise principled zoning over the full canvas. */
    float *Z = malloc((size_t)ntr * dim * sizeof(float));
    if (!Z) { fprintf(stderr, "oom features\n"); return 1; }
    printf("computing features...\n");
    for (long i = 0; i < ntr; i++) {
        const unsigned char *raw = tr_img + i * 784;
        if (use_raw) {
            for (int q = 0; q < 784; q++) Z[(size_t)i * dim + q] = (float)(255 - raw[q]) / 255.0f;
        } else {
            unsigned char inv[784];
            for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
            zoning_extract(zx, inv, 28, 28, Z + (size_t)i * dim);
        }
    }

    /* Standardize features (per-dimension mean/std, std-guarded). The ink
     * fraction cells are bounded in [0,1] but some dims are near-constant;
     * standardizing with a guarded std gives healthy pre-activation variance
     * so the hidden layer stays alive (the old collapse was a dead network). */
    float *zmean = malloc((size_t)dim * sizeof(float));
    float *zstd  = malloc((size_t)dim * sizeof(float));
    for (int d = 0; d < dim; d++) {
        double mu = 0;
        for (long i = 0; i < ntr; i++) mu += Z[(size_t)i * dim + d];
        mu /= (double)ntr; zmean[d] = (float)mu;
        double va = 0;
        for (long i = 0; i < ntr; i++) { double x = Z[(size_t)i * dim + d] - mu; va += x * x; }
        va /= (double)ntr;
        float sd = (float)sqrt(va);
        if (sd < 1e-2f) sd = 1.0f;     /* guard near-constant dims */
        zstd[d] = sd;
        for (long i = 0; i < ntr; i++)
            Z[(size_t)i * dim + d] = (Z[(size_t)i * dim + d] - zmean[d]) / zstd[d];
    }

    /* model */
    int do_plain = getenv("WUBUIX_PLAIN") ? 1 : 0;
    float clip_n = getenv("WUBUIX_CLIP") ? (float)atof(getenv("WUBUIX_CLIP")) : 5.0f;
    float base_lr = getenv("WUBUIX_LR") ? (float)atof(getenv("WUBUIX_LR")) : 0.008f;
    float mom     = getenv("WUBUIX_MOM") ? (float)atof(getenv("WUBUIX_MOM")) : 0.0f;
    float wd      = getenv("WUBUIX_WD")  ? (float)atof(getenv("WUBUIX_WD"))  : 0.0f;
    uint32_t seed = 0x1234ABCDu;
    MLP *m = mlp_create(dim, h1, h2, EMNIST_NCLASS, seed);

    /* Plain-path momentum buffers (one float* per parameter group). */
    float *vel[6] = {0};
    if (do_plain) {
        for (int g = 0; g < 6; g++) {
            int n = mlp_layer(m, g).n;
            vel[g] = (float *)calloc((size_t)n, sizeof(float));
        }
    }

    /* wubu Riemannian SGD optimizers (one per parameter group).
     * Default momentum 0 (clipped Euclidean SGD) for stability at this scale;
     * override with WUBUIX_MOM (momentum amplifies the effective LR, so keep
     * it small). The clip (max_grad_norm) is raised via WUBUIX_CLIP so the
     * optimizer is not throttled by per-sample gradient spikes. */
    WubuSGD o[6];
    if (!do_plain) {
        WubuSGDConfig cfg = {0};
        cfg.learning_rate = base_lr;
        cfg.momentum_factor = mom;
        cfg.weight_decay = wd;
        cfg.max_grad_norm = clip_n;
        cfg.q_controller_enabled = 0;
        WubuManifoldBinding man = {0}; man.manifold_enabled = 0;
        int sizes[6];
        for (int g = 0; g < 6; g++) sizes[g] = mlp_layer(m, g).n;
        for (int g = 0; g < 6; g++) wubu_sgd_init(&o[g], &cfg, man, sizes[g]);
    }

    WubuRNG rng; wubu_rng_init(&rng, 0x1234ABCDu);
    long batch = 256;
    long nbatch = (ntr + batch - 1) / batch;

    for (int ep = 0; ep < epochs; ep++) {
        /* constant LR (WuBu optimizer has clip+momentum built in). A short
         * linear cooldown in the final 20% of epochs helps settle. */
        float lr = base_lr;
        if (ep > (int)(epochs * 0.8f))
            lr = base_lr * 0.5f;
        if (do_plain) { /* plain handled below with its own clip */ }
        else for (int g = 0; g < 6; g++) wubu_sgd_set_lr(&o[g], lr);

        long *idx = malloc((size_t)ntr * sizeof(long));
        for (long i = 0; i < ntr; i++) idx[i] = i;
        for (long i = ntr - 1; i > 0; i--) {
            long j = (long)(wubu_rng_next(&rng) % (unsigned long)(i + 1));
            long t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
        for (long b = 0; b < nbatch; b++) {
            /* TRUE mini-batch: zero once, accumulate over `batch` samples,
             * average, then apply ONE update (10x+ less gradient noise than
             * per-sample SGD, so we can use a much larger stable LR). */
            mlp_zero_grad(m);
            long cnt = 0;
            for (long k = 0; k < batch && b * batch + k < ntr; k++, cnt++) {
                long s = idx[b * batch + k];
                int lab = tr_lab[s] - 1;
                if (lab < 0 || lab >= EMNIST_NCLASS) continue;
                const float *z = Z + (size_t)s * dim;
                mlp_forward(m, z, (float[EMNIST_NCLASS]){0});
                mlp_backward(m, z, lab);
            }
            if (cnt == 0) continue;
            mlp_scale_grad(m, 1.0f / (float)cnt);
            if (getenv("WUBUIX_DBG") && b == 0) {
                MLPLayer L = mlp_layer(m, 0);
                double gn = 0; for (int i = 0; i < L.n; i++) gn += L.grad[i]*L.grad[i];
                float sc[EMNIST_NCLASS]; int ph[EMNIST_NCLASS]={0};
                for (long t = 0; t < 500; t++) { mlp_forward(m, Z+(size_t)t*dim, sc);
                    int best=0; for(int c=1;c<EMNIST_NCLASS;c++) if(sc[c]>sc[best])best=c; ph[best]++; }
                int nd=0; for(int c=0;c<EMNIST_NCLASS;c++) if(ph[c])nd++;
                fprintf(stderr,"DBG ep0 step1 |gW1|=%.4f  init pred diversity=%d/26\n", sqrt(gn), nd);
            }
            if (do_plain) {
                for (int g = 0; g < 6; g++) {
                    MLPLayer L = mlp_layer(m, g);
                    float gn = 0;
                    for (int i = 0; i < L.n; i++) gn += L.grad[i] * L.grad[i];
                    gn = sqrtf(gn);
                    float scale = (gn > clip_n) ? clip_n / gn : 1.0f;
                    for (int i = 0; i < L.n; i++) {
                        float gv = L.grad[i] * scale;
                        vel[g][i] = mom * vel[g][i] + gv;
                        L.param[i] -= lr * vel[g][i];
                    }
                }
            } else {
                for (int g = 0; g < 6; g++) {
                    MLPLayer L = mlp_layer(m, g);
                    wubu_sgd_step_euclidean(&o[g], L.param, L.grad, L.n);
                }
            }
        }
        free(idx);

        /* honest train-accuracy + mean CE-loss proxy on a fixed 4000 prefix */
        long chk = ntr < 4000 ? ntr : 4000, cor = 0;
        double loss_sum = 0;
        float sc[EMNIST_NCLASS];
        for (long i = 0; i < chk; i++) {
            mlp_forward(m, Z + (size_t)i * dim, sc);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
            if (best == tr_lab[i] - 1) cor++;
            float mx = sc[0]; for (int c=1;c<EMNIST_NCLASS;c++) if(sc[c]>mx)mx=sc[c];
            float sm = 0; for (int c=0;c<EMNIST_NCLASS;c++) sm += expf(sc[c]-mx);
            int tgt = tr_lab[i]-1;
            loss_sum += -((sc[tgt] - mx) - logf(sm));   /* -log softmax[tgt] >= 0 */
        }
        float wnorm = 0.0f; float *W1 = mlp_layer(m, 0).param;
        for (int q = 0; q < dim * h1; q++) wnorm += W1[q] * W1[q];
        printf("  epoch %2d: train_acc~%.2f%%  loss=%.4f  lr=%.4f  |W1|=%.2f", ep + 1,
               100.0f * (float)cor / (float)chk, (float)(loss_sum/chk), lr, sqrtf(wnorm));
        /* periodic TRUE test-accuracy (capped for speed) every 10 epochs */
        if ((ep + 1) % 10 == 0) {
            long te_chk = nte < 4000 ? nte : 4000, te_cor = 0;
            float ts[EMNIST_NCLASS], tz[MAXFEAT];
            for (long i = 0; i < te_chk; i++) {
                const unsigned char *raw = te_img + i * 784;
                if (use_raw) { for (int q=0;q<784;q++) tz[q]=(float)(255-raw[q])/255.0f; }
                else { unsigned char inv2[784]; for (int q=0;q<784;q++) inv2[q]=(unsigned char)(255-raw[q]); zoning_extract(zx,inv2,28,28,tz); }
                for (int d=0;d<dim;d++) tz[d]=(tz[d]-zmean[d])/zstd[d];
                mlp_forward(m, tz, ts);
                int best=0; for(int c=1;c<EMNIST_NCLASS;c++) if(ts[c]>ts[best])best=c;
                if (best == te_lab[i]-1) te_cor++;
            }
            printf("  test@%ld=%.2f%%", te_chk, 100.0f*(float)te_cor/(float)te_chk);
        }
        printf("\n");
    }

    if (!do_plain) for (int g = 0; g < 6; g++) wubu_sgd_free(&o[g]);
    if (do_plain) for (int g = 0; g < 6; g++) free(vel[g]);

    /* ---- evaluate on full, balanced test set ---- */
    long correct = 0;
    long cc[EMNIST_NCLASS] = {0}, ct[EMNIST_NCLASS] = {0};
    long pred_count[EMNIST_NCLASS] = {0};
    unsigned char inv[784];
    float sc[EMNIST_NCLASS], z[MAXFEAT];
    for (long i = 0; i < nte; i++) {
        const unsigned char *raw = te_img + i * 784;
        if (use_raw) {
            for (int q = 0; q < 784; q++) z[q] = (float)(255 - raw[q]) / 255.0f;
        } else {
            unsigned char inv[784];
            for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
            zoning_extract(zx, inv, 28, 28, z);
        }
        for (int d = 0; d < dim; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
        mlp_forward(m, z, sc);
        int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
        int truth = te_lab[i] - 1;
        ct[truth]++; if (best == truth) { correct++; cc[truth]++; }
        pred_count[best]++;
    }
    float acc = 100.0f * (float)correct / (float)nte;
    long pmx = 0; for (int c = 0; c < EMNIST_NCLASS; c++) if (pred_count[c] > pmx) pmx = pred_count[c];
    printf("\n=== EMNIST Letters (wubu-trained lightweight MLP on zoning) ===\n");
    printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", acc, correct, nte);
    printf("COLLAPSE DIAG: most-predicted class = %.1f%% of test (3.85%% = full collapse)\n",
           100.0f * (float)pmx / (float)nte);
    printf("per-class:\n");
    for (int c = 0; c < EMNIST_NCLASS; c++)
        if (ct[c]) printf("  %c: %.1f%% (%ld/%ld)\n", 'A' + c,
                          100.0f * (float)cc[c] / (float)ct[c], cc[c], ct[c]);

    char wpath[576];
    snprintf(wpath, sizeof wpath, "%s/emnist_wubu_mlp_g%d_h%d_%d.wts", dir, grid, h1, h2);
    if (mlp_save(m, zmean, zstd, dim, wpath) == 0) printf("saved weights -> %s\n", wpath);

    mlp_destroy(m); zoning_destroy(zx);
    free(Z); free(zmean); free(zstd);
    free(tr_img); free(te_img); free(tr_lab); free(te_lab);
    return 0;
}
