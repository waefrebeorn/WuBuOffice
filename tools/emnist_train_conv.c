/* emnist_train_conv.c -- train an ULTRA-LIGHT conv+MLP on EMNIST Letters.
 *
 * Pipeline: load IDX corpus -> conv front-end (opaque ConvNet, scalar C11,
 * ~250k MACs/image, runs single-core on a 2007-era Q6600) -> MLP classifier
 * (opaque MLP) -> plain SGD end-to-end. The conv is trained TOGETHER with the
 * MLP: each sample does convnet_forward (fresh, fills cache) -> mlp_forward ->
 * mlp_backward -> mlp_input_grad (dL/dz) -> convnet_backward (uses the SAME
 * fresh cache). That is true end-to-end backprop, not frozen features.
 *
 * "Alternative reality where AI exists in 2011": pure C11, no SIMD/SSE
 * assumptions, no external deps. Optimizer is plain mini-batch SGD with
 * momentum + gradient clipping.
 *
 * Usage: emnist_train_conv <data/emnist> [h1] [h2] [epochs] [traincap] [lr] [mom]
 *   env: CN_LR, CN_MOM, CN_CLIP, CN_BATCH, CN_EPOCHS, CN_H1, CN_H2, CN_NORM
 * Saves: <data>/emnist_conv.wts  and  <data>/emnist_conv_mlp.wts
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#include "convnet.h"
#include "mlp.h"

#define IDX_MAGIC_IMAGE 0x00000803
#define MAXFEAT (16*16 + 16)

static long idx_count(const unsigned char *hdr) {
    return ((long)hdr[4] << 24) | ((long)hdr[5] << 16) |
           ((long)hdr[6] << 8)  | (long)hdr[7];
}
static int load_idx(const char *path, unsigned char **data, long *count) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    unsigned char hdr[16];
    if (fread(hdr, 1, 16, f) != 16) { fclose(f); return -1; }
    long n = idx_count(hdr);
    int pic = 1;
    if (hdr[3] == 3) {
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

static uint32_t s_rng = 0x1234ABCDu;
static long rnd_long(long mod) {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return (long)(s_rng % (unsigned long)mod);
}
/* In-place rotate a 28x28 glyph by `deg` degrees (nearest-ish bilinear),
 * used as cheap CPU data augmentation. EMNIST Letters are orientation-locked
 * and centered, so a small random rotation is the single biggest lever
 * against overfitting/collapse on this benchmark. */
static void rotate28(float *img, float *tmp, float deg) {
    float a = deg * (float)M_PI / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    float c = 13.5f;   /* rotation center (0..27 -> centered at 13.5) */
    for (int y = 0; y < 28; y++) for (int x = 0; x < 28; x++) {
        float dx = (float)x - c, dy = (float)y - c;
        float sx = dx * ca - dy * sa + c;
        float sy = dx * sa + dy * ca + c;
        int x0 = (int)floorf(sx), y0 = (int)floorf(sy);
        float fx = sx - x0, fy = sy - y0;
        float v = 0;
        for (int oy = 0; oy <= 1; oy++) for (int ox = 0; ox <= 1; ox++) {
            int xx = x0 + ox, yy = y0 + oy;
            if (xx < 0 || xx > 27 || yy < 0 || yy > 27) continue;
            float w = (ox ? fx : 1 - fx) * (oy ? fy : 1 - fy);
            v += w * img[yy * 28 + xx];
        }
        tmp[y * 28 + x] = v;
    }
    memcpy(img, tmp, 28 * 28 * sizeof(float));
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *dir = (argc > 1) ? argv[1] : "data/emnist";

    int h1 = getenv("CN_H1")     ? atoi(getenv("CN_H1"))     : 128;
    int h2 = getenv("CN_H2")     ? atoi(getenv("CN_H2"))     : 64;
    int epochs = getenv("CN_EPOCHS") ? atoi(getenv("CN_EPOCHS")) : (argc > 4 ? atoi(argv[4]) : 30);
    long traincap = (argc > 5) ? atol(argv[5]) : 0;
    float base_lr = getenv("CN_LR")  ? (float)atof(getenv("CN_LR"))  : 0.02f;
    float mom     = getenv("CN_MOM") ? (float)atof(getenv("CN_MOM")) : 0.9f;
    float clip_n  = getenv("CN_CLIP")? (float)atof(getenv("CN_CLIP")): 5.0f;
    long batch    = getenv("CN_BATCH")? atol(getenv("CN_BATCH"))     : 256;
    float aug_deg  = getenv("CN_AUG") ? (float)atof(getenv("CN_AUG")) : 0.0f;
    int do_norm   = getenv("CN_NORM") ? 1 : 0;
    (void)(argc > 2 ? atoi(argv[2]) : 0);
    (void)(argc > 3 ? atoi(argv[3]) : 0);
    (void)(argc > 6 ? atof(argv[6]) : 0.0);
    int label_off = getenv("CN_LABOFF") ? atoi(getenv("CN_LABOFF")) : 1;   /* EMNIST labels are 1..N; MNIST/Fashion/KMNIST are 0..N-1 */

    char ptr[512], pte[512], ptw[512], ptel[512];
    /* Dataset paths. Prefixes are the filename up to (but not including)
     * "-images"/"-labels". EMNIST Letters uses "...-train" / "...-test";
     * Fashion-MNIST uses "train" / "t10k". Pass via CN_TRAIN / CN_TEST
     * (relative to `dir`) or override fully with CN_PTR/CN_PTE/CN_PTW/CN_PTEL. */
    const char *trp = getenv("CN_TRAIN") ? getenv("CN_TRAIN") : "emnist/emnist-letters-train";
    const char *tep = getenv("CN_TEST")  ? getenv("CN_TEST")  : "emnist/emnist-letters-test";
    if (getenv("CN_PTR"))  snprintf(ptr,  sizeof ptr,  "%s", getenv("CN_PTR"));
    else snprintf(ptr,  sizeof ptr,  "%s/%s-images-idx3-ubyte",  dir, trp);
    if (getenv("CN_PTE"))  snprintf(pte,  sizeof pte,  "%s", getenv("CN_PTE"));
    else snprintf(pte,  sizeof pte,  "%s/%s-images-idx3-ubyte",  dir, tep);
    if (getenv("CN_PTW"))  snprintf(ptw,  sizeof ptw,  "%s", getenv("CN_PTW"));
    else snprintf(ptw,  sizeof ptw,  "%s/%s-labels-idx1-ubyte",  dir, trp);
    if (getenv("CN_PTEL")) snprintf(ptel, sizeof ptel, "%s", getenv("CN_PTEL"));
    else snprintf(ptel, sizeof ptel, "%s/%s-labels-idx1-ubyte", dir, tep);

    unsigned char *tr_img = NULL, *te_img = NULL, *tr_lab = NULL, *te_lab = NULL;
    long ntr = 0, nte = 0, ntw = 0, nte_l = 0;
    if (load_idx(ptr, &tr_img, &ntr)) return 1;
    if (load_idx(pte, &te_img, &nte)) return 1;
    if (load_idx(ptw, &tr_lab, &ntw)) return 1;
    if (load_idx(ptel, &te_lab, &nte_l)) return 1;
    if (traincap && traincap < ntr) ntr = traincap;
    if (ntr > ntw) ntr = ntw;
    nte = nte_l;

    int nclass = getenv("CN_CLASS") ? atoi(getenv("CN_CLASS")) : 26;
    ConvNet *cn = convnet_create(&CONV_LIGHT);
    int D = convnet_dim(cn);
    MLP *m = mlp_create(D, h1, h2, nclass, 0x1234ABCDu);
    int use_adam = getenv("CN_OPT") ? (strcmp(getenv("CN_OPT"), "sgd") != 0) : 0;   /* default SGD */
    float eff_lr = base_lr;
    if (use_adam && !getenv("CN_LR")) eff_lr = 0.002f;   /* Adam default */
    float conv_fac = getenv("CN_CONVF") ? (float)atof(getenv("CN_CONVF")) : 0.1f;
    long tstep = 0;
    const float b1 = 0.9f, b2 = 0.999f, eps = 1e-8f;
    printf("conv+MLP: conv features=%d -> MLP %d->%d->%d->%d; epochs=%d batch=%ld lr=%.4f %s norm=%d nclass=%d; train=%ld test=%ld\n",
           D, D, h1, h2, nclass, epochs, batch, eff_lr, use_adam ? "adam" : "sgd", do_norm, nclass, ntr, nte);

    /* Optional per-dim standardization of conv features, estimated ONCE over
     * the (initial) conv outputs. Conv maxpool activations are bounded and
     * non-negative, so the net trains fine without it; CN_NORM=1 enables it
     * for extra hidden-layer headroom. */
    float *zmean = malloc((size_t)D * sizeof(float));
    float *zstd  = malloc((size_t)D * sizeof(float));
    if (do_norm) {
        double *sum = calloc((size_t)D, sizeof(double));
        double *sum2 = calloc((size_t)D, sizeof(double));
        for (long i = 0; i < ntr; i++) {
            const unsigned char *raw = tr_img + i * 784;
            float im[784];
            for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
            float ft[256]; convnet_forward(cn, im, ft);
            for (int d = 0; d < D; d++) { sum[d] += ft[d]; sum2[d] += (double)ft[d]*ft[d]; }
        }
        for (int d = 0; d < D; d++) {
            double mu = sum[d] / (double)ntr;
            double va = sum2[d] / (double)ntr - mu*mu;
            float sd = (float)sqrt(va > 0 ? va : 1.0);
            if (sd < 1e-2f) sd = 1.0f;
            zmean[d] = (float)mu; zstd[d] = sd;
        }
        free(sum); free(sum2);
    } else {
        for (int d = 0; d < D; d++) { zmean[d] = 0.0f; zstd[d] = 1.0f; }
    }

    /* plain-SGD momentum buffers + Adam moments for conv + mlp */
    float *cvel[4] = {0}, *cmsq[4] = {0};
    for (int g = 0; g < convnet_layer_count(cn); g++) {
        int n = convnet_layer(cn, g).n;
        cvel[g] = (float *)calloc((size_t)n, sizeof(float));
        cmsq[g] = (float *)calloc((size_t)n, sizeof(float));
    }
    float *mvel[6] = {0}, *mmsso[6] = {0};
    for (int g = 0; g < 6; g++) {
        int n = mlp_layer(m, g).n;
        mvel[g] = (float *)calloc((size_t)n, sizeof(float));
        mmsso[g] = (float *)calloc((size_t)n, sizeof(float));
    }

    long nbatch = (ntr + batch - 1) / batch;
    float *feat = malloc((size_t)D * sizeof(float));
    float *df   = malloc((size_t)D * sizeof(float));

    for (int ep = 0; ep < epochs; ep++) {
        float lr = use_adam ? eff_lr : base_lr;
        if (ep > (int)(epochs * 0.8f)) lr = lr * 0.5f;   /* gentle cooldown */

        long *idx = malloc((size_t)ntr * sizeof(long));
        for (long i = 0; i < ntr; i++) idx[i] = i;
        for (long i = ntr - 1; i > 0; i--) {
            long j = rnd_long(i + 1);
            long t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
        for (long b = 0; b < nbatch; b++) {
            convnet_zero_grad(cn); mlp_zero_grad(m);
            long cnt = 0;
            for (long k = 0; k < batch && b * batch + k < ntr; k++, cnt++) {
                long n = idx[b * batch + k];
                int lab = tr_lab[n] - label_off;
                if (lab < 0 || lab >= nclass) continue;
                /* FRESH conv forward (fills cache + feat). Optional rotation
                 * augmentation (CN_AUG deg) breaks orientation-lock overfit. */
                const unsigned char *raw = tr_img + n * 784;
                float im[784], imt[784];
                for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
                if (aug_deg > 0.0f) {
                    float deg = ((float)(rnd_long(1000) % 1000) / 1000.0f * 2.0f - 1.0f) * aug_deg;
                    rotate28(im, imt, deg);
                }
                convnet_forward(cn, im, feat);
                if (do_norm) for (int d = 0; d < D; d++) feat[d] = (feat[d] - zmean[d]) / zstd[d];
                /* MLP forward/backward on the SAME features */
                float sc[nclass];
                mlp_forward(m, feat, sc);
                mlp_backward(m, feat, lab);
                mlp_input_grad(m, feat, df);
                /* conv backward uses the fresh cached activations from the
                 * convnet_forward above (end-to-end, NOT frozen). */
                convnet_backward(cn, im, feat, df);
            }
            if (cnt == 0) continue;
            mlp_scale_grad(m, 1.0f / (float)cnt);
            convnet_scale_grad(cn, 1.0f / (float)cnt);

            /* ---- MLP update ---- */
            if (use_adam) {
                tstep++;
                float corr = sqrtf(1.0f - powf(b2, (float)tstep)) / (1.0f - powf(b1, (float)tstep));
                for (int g = 0; g < 6; g++) {
                    MLPLayer L = mlp_layer(m, g);
                    for (int i = 0; i < L.n; i++) {
                        float gv = L.grad[i];
                        mvel[g][i] = b1 * mvel[g][i] + (1.0f - b1) * gv;
                        mmsso[g][i] = b2 * mmsso[g][i] + (1.0f - b2) * gv * gv;
                        float uh = mvel[g][i] / (sqrtf(mmsso[g][i]) + eps);
                        L.param[i] -= lr * corr * uh;
                    }
                }
            } else {
                for (int g = 0; g < 6; g++) {
                    MLPLayer L = mlp_layer(m, g);
                    float gn = 0; for (int i = 0; i < L.n; i++) gn += L.grad[i]*L.grad[i];
                    gn = sqrtf(gn);
                    float scale = (gn > clip_n) ? clip_n / gn : 1.0f;
                    for (int i = 0; i < L.n; i++) {
                        float gv = L.grad[i] * scale;
                        mvel[g][i] = mom * mvel[g][i] + gv;
                        L.param[i] -= lr * mvel[g][i];
                    }
                }
            }
            /* ---- conv update ---- */
            if (use_adam) {
                float corr = sqrtf(1.0f - powf(b2, (float)tstep)) / (1.0f - powf(b1, (float)tstep));
                float clr = lr * conv_fac;
                for (int g = 0; g < convnet_layer_count(cn); g++) {
                    ConvLayer L = convnet_layer(cn, g);
                    for (int i = 0; i < L.n; i++) {
                        float gv = L.grad[i];
                        cvel[g][i] = b1 * cvel[g][i] + (1.0f - b1) * gv;
                        cmsq[g][i] = b2 * cmsq[g][i] + (1.0f - b2) * gv * gv;
                        float uh = cvel[g][i] / (sqrtf(cmsq[g][i]) + eps);
                        L.param[i] -= clr * corr * uh;
                    }
                }
            } else {
                float clr = lr * conv_fac;
                for (int g = 0; g < convnet_layer_count(cn); g++) {
                    ConvLayer L = convnet_layer(cn, g);
                    float gn = 0; for (int i = 0; i < L.n; i++) gn += L.grad[i]*L.grad[i];
                    gn = sqrtf(gn);
                    float scale = (gn > clip_n) ? clip_n / gn : 1.0f;
                    for (int i = 0; i < L.n; i++) {
                        float gv = L.grad[i] * scale;
                        cvel[g][i] = mom * cvel[g][i] + gv;
                        L.param[i] -= clr * cvel[g][i];
                    }
                }
            }
        }
        free(idx);

        /* conv-alive probe (CN_DIAG): fraction of test conv outputs > 0.
         * If this collapses to ~0 mid-training the conv has died and the
         * run is wasted; catch it early instead of waiting 40 epochs. */
        if (getenv("CN_DIAG")) {
            long dpv = 0, tot = 0; float zp[256];
            for (long i = 0; i < nte && i < 500; i++) {
                const unsigned char *raw = te_img + i * 784;
                float im[784];
                for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
                convnet_forward(cn, im, zp);
                for (int d = 0; d < D; d++) { tot++; if (zp[d] > 0) dpv++; }
            }
            fprintf(stderr, "[CN_DIAG] ep%d conv alive %% = %.1f\n", ep + 1, 100.0f * (float)dpv / (float)tot);
        }

        /* train-accuracy on a 4000 prefix */
        long chk = ntr < 4000 ? ntr : 4000, cor = 0;
        float sc[nclass];
        for (long i = 0; i < chk; i++) {
            const unsigned char *raw = tr_img + i * 784;
            float im[784];
            for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
            convnet_forward(cn, im, feat);
            if (do_norm) for (int d = 0; d < D; d++) feat[d] = (feat[d] - zmean[d]) / zstd[d];
            mlp_forward(m, feat, sc);
            int best = 0; for (int c = 1; c < nclass; c++) if (sc[c] > sc[best]) best = c;
            if (best == tr_lab[i] - label_off) cor++;
        }
        printf("  epoch %2d: train_acc~%.2f%%  lr=%.4f\n", ep + 1, 100.0f * (float)cor / (float)chk, lr);
    }

    free(feat); free(df);
    for (int g = 0; g < 6; g++) { free(mvel[g]); free(mmsso[g]); }
    for (int g = 0; g < convnet_layer_count(cn); g++) { free(cvel[g]); free(cmsq[g]); }

    /* ---- full test accuracy ---- */
    long correct = 0;
    float sc[nclass], z[MAXFEAT];
    for (long i = 0; i < nte; i++) {
        const unsigned char *raw = te_img + i * 784;
        float im[784];
        for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
        convnet_forward(cn, im, z);
        if (do_norm) for (int d = 0; d < D; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
        mlp_forward(m, z, sc);
        int best = 0; for (int c = 1; c < nclass; c++) if (sc[c] > sc[best]) best = c;
        if (best == te_lab[i] - label_off) correct++;
    }
    float acc = 100.0f * (float)correct / (float)nte;
    printf("\n=== EMNIST Letters (ultra-light conv+MLP, plain C11 SGD) ===\n");
    printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", acc, correct, nte);

    char cpath[576], mpath[576];
    snprintf(cpath, sizeof cpath, "%s/emnist_conv.wts", dir);
    snprintf(mpath, sizeof mpath, "%s/emnist_conv_mlp.wts", dir);
    if (convnet_save(cn, cpath) == 0) printf("saved conv -> %s\n", cpath);
    if (mlp_save(m, zmean, zstd, D, mpath) == 0) printf("saved mlp  -> %s\n", mpath);

    mlp_destroy(m); convnet_destroy(cn);
    free(zmean); free(zstd);
    free(tr_img); free(te_img); free(tr_lab); free(te_lab);
    return 0;
}
