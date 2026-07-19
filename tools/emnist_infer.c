/* emnist_infer.c -- lightweight C11 OCR inference (dependency-free).
 *
 * Loads a wubu-trained MLP (see emnist_train.c) and classifies EMNIST Letters.
 *   emnist_infer <data/emnist> <weights.wts>        -> full test-set accuracy
 *   emnist_infer <data/emnist> <weights.wts> N      -> classify first N, print A-Z
 *
 * No wubu math needed at inference time: only zoning + mlp modules.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "zoning.h"
#include "mlp.h"

#define EMNIST_NCLASS 26
#define MAXFEAT (64*64 + 8)

static long idx_count(const unsigned char *hdr) {
    return ((long)hdr[4] << 24) | ((long)hdr[5] << 16) |
           ((long)hdr[6] << 8)  | (long)hdr[7];
}
static int load_idx(const char *path, unsigned char **data, long *count) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
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
    if (argc < 3) {
        fprintf(stderr, "usage: %s <data/emnist> <weights.wts> [N]\n", argv[0]);
        return 1;
    }
    const char *dir = argv[1];
    const char *wpath = argv[2];
    long show = (argc > 3) ? atol(argv[3]) : 0;

    float zmean[MAXFEAT], zstd[MAXFEAT];
    int dim = 0;
    MLP *m = NULL;
    if (mlp_load(wpath, &m, zmean, zstd, &dim) != 0) {
        fprintf(stderr, "failed to load weights %s\n", wpath);
        return 1;
    }
    ZoningExtractor *zx = zoning_create(0);
    /* grid is implied by dim; recreate with correct grid to be safe */
    int grid = 0;
    /* dim = grid*grid + 8 -> solve for grid */
    int g = 1; while (g * g + 8 < dim) g++;
    grid = g;
    zoning_destroy(zx);
    zx = zoning_create(grid);
    if (zoning_dim(zx) != dim) {
        fprintf(stderr, "feature dim mismatch: model %d vs zoning %d (grid %d)\n",
                dim, zoning_dim(zx), grid);
        return 1;
    }

    char pte[512], ptel[512];
    snprintf(pte, sizeof pte, "%s/emnist-letters-test-images-idx3-ubyte", dir);
    snprintf(ptel, sizeof ptel, "%s/emnist-letters-test-labels-idx1-ubyte", dir);
    unsigned char *te_img = NULL, *te_lab = NULL;
    long nte = 0, nte_l = 0;
    if (load_idx(pte, &te_img, &nte)) return 1;
    if (load_idx(ptel, &te_lab, &nte_l)) return 1;
    nte = nte_l;

    unsigned char inv[784];
    float z[MAXFEAT], sc[EMNIST_NCLASS];

    if (show > 0) {
        for (long i = 0; i < show && i < nte; i++) {
            const unsigned char *raw = te_img + i * 784;
            for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
            zoning_extract(zx, inv, 28, 28, z);
            for (int d = 0; d < dim; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
            mlp_forward(m, z, sc);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
            int truth = te_lab[i] - 1;
            printf("sample %4ld: pred=%c truth=%c %s\n", i, 'A' + best, 'A' + truth,
                   best == truth ? "OK" : "XX");
        }
    } else {
        long correct = 0;
        long cc[EMNIST_NCLASS] = {0}, ct[EMNIST_NCLASS] = {0};
        for (long i = 0; i < nte; i++) {
            const unsigned char *raw = te_img + i * 784;
            for (int q = 0; q < 784; q++) inv[q] = (unsigned char)(255 - raw[q]);
            zoning_extract(zx, inv, 28, 28, z);
            for (int d = 0; d < dim; d++) z[d] = (z[d] - zmean[d]) / zstd[d];
            mlp_forward(m, z, sc);
            int best = 0; for (int c = 1; c < EMNIST_NCLASS; c++) if (sc[c] > sc[best]) best = c;
            int truth = te_lab[i] - 1;
            ct[truth]++; if (best == truth) { correct++; cc[truth]++; }
        }
        float acc = 100.0f * (float)correct / (float)nte;
        printf("=== EMNIST Letters inference (wubu OCR MLP) ===\n");
        printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n", acc, correct, nte);
        printf("per-class:\n");
        for (int c = 0; c < EMNIST_NCLASS; c++)
            if (ct[c]) printf("  %c: %.1f%% (%ld/%ld)\n", 'A' + c,
                              100.0f * (float)cc[c] / (float)ct[c], cc[c], ct[c]);
    }

    mlp_destroy(m); zoning_destroy(zx);
    free(te_img); free(te_lab);
    return 0;
}
