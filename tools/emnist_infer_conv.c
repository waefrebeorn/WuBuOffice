/* emnist_infer_conv.c -- ultra-light conv+MLP OCR inference (dependency-free).
 *
 * Loads convnet.wts (ConvNet) + emnist_conv_mlp.wts (MLP + standardization
 * stats) produced by emnist_train_conv, and classifies EMNIST Letters.
 *   emnist_infer_conv <data/emnist>            -> full test-set accuracy
 *   emnist_infer_conv <data/emnist> N          -> classify first N, print A-Z
 *
 * Pure C11, no wubu math: only convnet + mlp modules. Runs single-core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "convnet.h"
#include "mlp.h"

#define EMNIST_NCLASS 26
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
    *count = (long)(got / pic); *data = buf; fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <data/emnist> [N]\n", argv[0]);
        return 1;
    }
    const char *dir = argv[1];
    long show = (argc > 2) ? atol(argv[2]) : 0;

    char cpath[576], mpath[576];
    snprintf(cpath, sizeof cpath, "%s/emnist_conv.wts", dir);
    snprintf(mpath, sizeof mpath, "%s/emnist_conv_mlp.wts", dir);

    ConvNet *cn = NULL; ConvConfig ccfg;
    if (convnet_load(cpath, &cn, &ccfg) != 0) {
        fprintf(stderr, "failed to load conv weights %s\n", cpath);
        return 1;
    }
    int D = convnet_dim(cn);

    float zmean[MAXFEAT], zstd[MAXFEAT];
    int dim = 0;
    MLP *m = NULL;
    if (mlp_load(mpath, &m, zmean, zstd, &dim) != 0) {
        fprintf(stderr, "failed to load mlp weights %s\n", mpath);
        convnet_destroy(cn); return 1;
    }
    if (dim != D) {
        fprintf(stderr, "conv/mlp dim mismatch: conv=%d mlp=%d\n", D, dim);
        mlp_destroy(m); convnet_destroy(cn); return 1;
    }

    char pte[512], ptel[512];
    snprintf(pte, sizeof pte, "%s/emnist-letters-test-images-idx3-ubyte", dir);
    snprintf(ptel, sizeof ptel, "%s/emnist-letters-test-labels-idx1-ubyte", dir);
    unsigned char *te_img = NULL, *te_lab = NULL;
    long nte = 0, nte_l = 0;
    if (load_idx(pte, &te_img, &nte)) return 1;
    if (load_idx(ptel, &te_lab, &nte_l)) return 1;
    nte = nte_l;

    if (show > 0) {
        float z[MAXFEAT], sc[EMNIST_NCLASS];
        for (long i = 0; i < show && i < nte; i++) {
            const unsigned char *raw = te_img + i * 784;
            float im[784];
            for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
            convnet_forward(cn, im, z);
            for (int d = 0; d < D; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
            mlp_forward(m, z, sc);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
            int truth = te_lab[i] - 1;
            printf("sample %4ld: pred=%c truth=%c %s\n", i, 'A' + best, 'A' + truth,
                   best == truth ? "OK" : "XX");
        }
    } else {
        long correct = 0;
        long cc[EMNIST_NCLASS] = {0}, ct[EMNIST_NCLASS] = {0};
        float z[MAXFEAT], sc[EMNIST_NCLASS];
        for (long i = 0; i < nte; i++) {
            const unsigned char *raw = te_img + i * 784;
            float im[784];
            for (int q = 0; q < 784; q++) im[q] = (float)(255 - raw[q]) / 255.0f;
            convnet_forward(cn, im, z);
            for (int d = 0; d < D; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
            mlp_forward(m, z, sc);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
            int truth = te_lab[i] - 1;
            ct[truth]++; if (best == truth) { correct++; cc[truth]++; }
        }
        float acc = 100.0f * (float)correct / (float)nte;
        printf("=== EMNIST Letters inference (ultra-light conv+MLP) ===\n");
        printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", acc, correct, nte);
        printf("per-class:\n");
        for (int c = 0; c < EMNIST_NCLASS; c++)
            if (ct[c]) printf("  %c: %.1f%% (%ld/%ld)\n", 'A' + c,
                              100.0f * (float)cc[c] / (float)ct[c], cc[c], ct[c]);
    }

    mlp_destroy(m); convnet_destroy(cn);
    free(te_img); free(te_lab);
    return 0;
}
