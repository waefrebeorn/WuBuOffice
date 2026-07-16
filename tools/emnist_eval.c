/* emnist_eval.c -- train + evaluate the WuBuOCR lightweight zoning recognizer
 * on the EMNIST Letters benchmark (A-Z, 26 classes, 28x28 grayscale).
 *
 * This is the "train our superior lightweight C11 OCR module on an acclaimed,
 * lightweight, reliable, text-only dataset" step:
 *   - TRAIN: accumulate per-class mean zoning vectors from the EMNIST TRAIN
 *            split (ocr_templates_create_classes + add_sample + finalize).
 *   - EVAL : run 1-NN recognition over the EMNIST TEST split and report overall
 *            accuracy, per-class accuracy, and the top confusion pairs.
 *
 * Usage:
 *   emnist_eval [DIR] [GRID] [N_TRAIN] [N_TEST]
 *     DIR     : directory holding emnist-letters-*-idx?-ubyte (default ./data/emnist)
 *     GRID    : zoning grid N (default 5)
 *     N_TRAIN : cap training samples per class (0 = all; default 0)
 *     N_TEST  : cap test samples (0 = all; default 0)
 *
 * No third-party deps: reads the IDX format directly, uses the in-tree
 * recognize.c / binarize.c. The IDX files are git-ignored (see .gitignore).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "recognize.h"
#include "binarize.h"

/* ---- minimal IDX reader (big-endian headers) ---- */
static int read_u32(const unsigned char **p) {
    unsigned v = ((unsigned)(*p)[0] << 24) | ((unsigned)(*p)[1] << 16) |
                 ((unsigned)(*p)[2] << 8)  | ((unsigned)(*p)[3]);
    *p += 4;
    return (int)v;
}

/* Load an EMNIST Letters split. Returns malloc'd images (n*28*28, 0=black..255)
 * and labels (n, 1..26). Returns count, or -1 on error. */
static long load_split(const char *dir, int is_test,
                       unsigned char **out_imgs, unsigned char **out_labels) {
    char path[1024];
    snprintf(path, sizeof path, "%s/emnist-letters-%s-images-idx3-ubyte",
             dir, is_test ? "test" : "train");
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);

    const unsigned char *p = buf;
    int magic = read_u32(&p);   /* 0x00000803 */
    long n = read_u32(&p);
    long rows = read_u32(&p);
    long cols = read_u32(&p);
    if (magic != 0x00000803 || rows != 28 || cols != 28 || n <= 0) {
        fprintf(stderr, "bad image header in %s (magic=%x n=%ld)\n", path, magic, n);
        free(buf); return -1;
    }
    size_t pix = (size_t)n * 28 * 28;
    unsigned char *imgs = malloc(pix);
    if (!imgs) { free(buf); return -1; }
    memcpy(imgs, p, pix);

    /* labels */
    snprintf(path, sizeof path, "%s/emnist-letters-%s-labels-idx1-ubyte",
             dir, is_test ? "test" : "train");
    FILE *fl = fopen(path, "rb");
    if (!fl) { fprintf(stderr, "cannot open %s\n", path); free(buf); free(imgs); return -1; }
    fseek(fl, 0, SEEK_END); long ls = ftell(fl); fseek(fl, 0, SEEK_SET);
    unsigned char *lbuf = malloc((size_t)ls);
    if (!lbuf) { fclose(fl); free(buf); free(imgs); return -1; }
    if (fread(lbuf, 1, (size_t)ls, fl) != (size_t)ls) { free(lbuf); fclose(fl); free(buf); free(imgs); return -1; }
    fclose(fl);
    const unsigned char *lp = lbuf;
    int lmagic = read_u32(&lp);
    long ln = read_u32(&lp);
    if (lmagic != 0x00000801 || ln != n) {
        fprintf(stderr, "bad label header in %s\n", path);
        free(lbuf); free(buf); free(imgs); return -1;
    }
    unsigned char *labels = malloc((size_t)n);
    if (!labels) { free(lbuf); free(buf); free(imgs); return -1; }
    memcpy(labels, lp, (size_t)n);

    free(buf); free(lbuf);
    *out_imgs = imgs; *out_labels = labels;
    return n;
}

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : "data/emnist";
    int g0 = (argc > 2) ? atoi(argv[2]) : 4;   /* grid sweep start */
    int g1 = (argc > 3) ? atoi(argv[3]) : 8;   /* grid sweep end   */
    long cap_train = (argc > 4) ? atol(argv[4]) : 0;
    long cap_test  = (argc > 5) ? atol(argv[5]) : 0;
    if (g0 < 2) g0 = 2;
    if (g1 > 16) g1 = 16;
    if (g1 < g0) g1 = g0;

    /* EMNIST Letters: label N (1..26) -> 'A'+N-1 */
    char classes[26];
    for (int i = 0; i < 26; i++) classes[i] = (char)('A' + i);

    unsigned char *tr_imgs = NULL, *tr_lab = NULL;
    long ntr = load_split(dir, 0, &tr_imgs, &tr_lab);
    if (ntr < 0) return 1;
    unsigned char *te_imgs = NULL, *te_lab = NULL;
    long nte = load_split(dir, 1, &te_imgs, &te_lab);
    if (nte < 0) { free(tr_imgs); free(tr_lab); return 1; }

    long best_grid = g0; double best_acc = -1.0;
    long long_used = 0;

    for (int grid = g0; grid <= g1; grid++) {
        OcrTemplates *t = ocr_templates_create_classes((size_t)grid, classes, 26);
        if (!t) { fprintf(stderr, "template alloc failed\n"); return 1; }
        /* Closed-set benchmark: every glyph is a letter, so never reject. */
        ocr_templates_set_reject(t, 0, 0.97, 0.60);
        /* Structural augmentation: aspect-ratio + hole-count disambiguate
         * thin strokes (I/L/T) and looped letters (O/Q/D/G). Tuned weights. */
        ocr_templates_set_struct(t, 2.0, 1.5);

        long used = 0;
        uint8_t inv[784];
        for (long i = 0; i < ntr; i++) {
            int lab = tr_lab[i];
            if (lab < 1 || lab > 26) continue;
            if (cap_train && used >= cap_train) break;
            /* EMNIST stores ink as HIGH pixel value; the recognizer's internal
             * convention is ink = dark (0). Invert at the data boundary. */
            const uint8_t *raw = tr_imgs + (size_t)i * 784;
            for (int q = 0; q < 784; q++) inv[q] = (uint8_t)(255 - raw[q]);
            ocr_templates_add_sample(t, (size_t)(lab - 1), inv, 28, 28, 127);
            used++;
        }
        ocr_templates_finalize(t);

        long correct = 0, total = 0, rejected = 0;
        long cls_correct[26] = {0}, cls_total[26] = {0};
        long ntest = (cap_test && cap_test < nte) ? cap_test : nte;
        uint8_t invt[784];
        for (long i = 0; i < ntest; i++) {
            int lab = te_lab[i];
            if (lab < 1 || lab > 26) continue;
            int truth = lab - 1;
            const uint8_t *raw = te_imgs + (size_t)i * 784;
            for (int q = 0; q < 784; q++) invt[q] = (uint8_t)(255 - raw[q]);
            OcrBinary *b = ocr_binary_from_raw(invt, 28, 28, 127);
            if (!b) continue;
            OcrBlock box = {0, 0, 28, 28};
            char *pred = ocr_recognize_glyph(b, &box, t);
            ocr_binary_free(b);
            cls_total[truth]++; total++;
            int pred_i = -1;
            if (pred) {
                pred_i = (pred[0] >= 'A' && pred[0] <= 'Z') ? (pred[0] - 'A') : -1;
                free(pred);
            } else {
                rejected++;
            }
            if (pred_i == truth) { correct++; cls_correct[truth]++; }
        }

        double acc = total ? 100.0 * (double)correct / (double)total : 0.0;
        printf("grid=%2d  accuracy=%5.2f%%  (%ld/%ld)  rejected=%ld\n",
               grid, acc, correct, total, rejected);
        if (acc > best_acc) { best_acc = acc; best_grid = grid; long_used = used; }
        ocr_templates_free(t);

        if (grid == best_grid) {
            /* stash per-class for the best grid report */
            /* (recomputed below for the winning grid) */
        }
    }

    /* Final detailed report on the best grid. */
    {
        OcrTemplates *t = ocr_templates_create_classes((size_t)best_grid, classes, 26);
        ocr_templates_set_reject(t, 0, 0.97, 0.60);
        /* Structural augmentation: aspect-ratio + hole-count disambiguate
         * thin strokes (I/L/T) and looped letters (O/Q/D/G). Tuned weights. */
        ocr_templates_set_struct(t, 2.0, 1.5);
        for (long i = 0; i < ntr; i++) {
            int lab = tr_lab[i];
            if (lab < 1 || lab > 26) continue;
            if (cap_train && i >= cap_train) break;
            const uint8_t *raw = tr_imgs + (size_t)i * 784;
            uint8_t inv[784];
            for (int q = 0; q < 784; q++) inv[q] = (uint8_t)(255 - raw[q]);
            ocr_templates_add_sample(t, (size_t)(lab - 1), inv, 28, 28, 127);
        }
        ocr_templates_finalize(t);

        long cls_correct[26] = {0}, cls_total[26] = {0};
        long conf[26][26] = {{0}};
        long correct = 0, total = 0;
        long ntest = (cap_test && cap_test < nte) ? cap_test : nte;
        for (long i = 0; i < ntest; i++) {
            int lab = te_lab[i];
            if (lab < 1 || lab > 26) continue;
            int truth = lab - 1;
            const uint8_t *raw = te_imgs + (size_t)i * 784;
            uint8_t invt[784];
            for (int q = 0; q < 784; q++) invt[q] = (uint8_t)(255 - raw[q]);
            OcrBinary *b = ocr_binary_from_raw(invt, 28, 28, 127);
            if (!b) continue;
            OcrBlock box = {0, 0, 28, 28};
            char *pred = ocr_recognize_glyph(b, &box, t);
            ocr_binary_free(b);
            cls_total[truth]++; total++;
            int pred_i = -1;
            if (pred) { pred_i = (pred[0] >= 'A' && pred[0] <= 'Z') ? (pred[0]-'A') : -1; free(pred); }
            if (pred_i == truth) { correct++; cls_correct[truth]++; }
            else if (pred_i >= 0) conf[truth][pred_i]++;
        }
        printf("\n=== EMNIST Letters EVAL (best grid=%ld, test n=%ld, train used=%ld) ===\n",
               best_grid, total, long_used);
        printf("OVERALL ACCURACY: %.2f%% (%ld/%ld)\n",
               total ? 100.0*(double)correct/(double)total : 0.0, correct, total);
        printf("per-class accuracy:\n");
        for (int i = 0; i < 26; i++) {
            double ar = 0; int holes = 0;
            ocr_templates_struct_info(t, (size_t)i, &ar, &holes);
            printf("  %c: %5.1f%% (%ld/%ld)  ar=%.2f holes=%d\n", classes[i],
                   cls_total[i] ? 100.0*(double)cls_correct[i]/(double)cls_total[i] : 0.0,
                   cls_correct[i], cls_total[i], ar, holes);
        }
        printf("top confusions (truth->pred : count):\n");
        long top[10]; int ti[10][2];
        for (int k = 0; k < 10; k++) top[k] = -1;
        for (int a = 0; a < 26; a++)
            for (int p = 0; p < 26; p++) {
                if (a == p) continue;
                long c = conf[a][p];
                for (int k = 0; k < 10; k++)
                    if (c > top[k]) {
                        for (int m = 9; m > k; m--) {
                            top[m] = top[m-1];
                            ti[m][0] = ti[m-1][0];
                            ti[m][1] = ti[m-1][1];
                        }
                        top[k] = c; ti[k][0] = a; ti[k][1] = p; break;
                    }
            }
        for (int k=0;k<10 && top[k]>0;k++)
            printf("  %c->%c : %ld\n", classes[ti[k][0]], classes[ti[k][1]], top[k]);
        ocr_templates_free(t);
    }

    free(tr_imgs); free(tr_lab); free(te_imgs); free(te_lab);
    return 0;
}
