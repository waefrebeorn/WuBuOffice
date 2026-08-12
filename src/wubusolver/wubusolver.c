#include "wubusolver.h"
#include <stdlib.h>
#include <math.h>

static double *row(int n) { return (double *)malloc((size_t)n * n * sizeof(double)); }

int wubusolver_solve(const double *A, int n, const double *b, double *x) {
    if (!A || !b || !x || n < 1) return -1;
    /* augmented copy with partial pivoting + back substitution */
    double *aug = (double *)malloc((size_t)n * (n + 1) * sizeof(double));
    if (!aug) return -1;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) aug[i * (n + 1) + j] = A[i * n + j];
        aug[i * (n + 1) + n] = b[i];
    }
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(aug[r * (n + 1) + col]) > fabs(aug[piv * (n + 1) + col])) piv = r;
        if (fabs(aug[piv * (n + 1) + col]) < 1e-12) { free(aug); return -1; }
        if (piv != col) for (int j = 0; j <= n; j++) {
            double t = aug[col * (n + 1) + j]; aug[col * (n + 1) + j] = aug[piv * (n + 1) + j]; aug[piv * (n + 1) + j] = t;
        }
        double d = aug[col * (n + 1) + col];
        for (int j = 0; j <= n; j++) aug[col * (n + 1) + j] /= d;
        for (int r = 0; r < n; r++) {
            if (r == col) continue;
            double f = aug[r * (n + 1) + col];
            if (f != 0) for (int j = 0; j <= n; j++) aug[r * (n + 1) + j] -= f * aug[col * (n + 1) + j];
        }
    }
    for (int i = 0; i < n; i++) x[i] = aug[i * (n + 1) + n];
    free(aug);
    return 0;
}

double wubusolver_det(const double *A, int n) {
    if (!A || n < 1) return 0;
    double *a = (double *)malloc((size_t)n * n * sizeof(double));
    if (!a) return 0;
    for (int i = 0; i < n * n; i++) a[i] = A[i];
    double det = 1.0; int swaps = 0;
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(a[r * n + col]) > fabs(a[piv * n + col])) piv = r;
        if (fabs(a[piv * n + col]) < 1e-12) { free(a); return 0; }
        if (piv != col) { for (int j = 0; j < n; j++) { double t = a[col * n + j]; a[col * n + j] = a[piv * n + j]; a[piv * n + j] = t; } swaps++; }
        det *= a[col * n + col];
        for (int r = col + 1; r < n; r++) {
            double f = a[r * n + col] / a[col * n + col];
            for (int j = col; j < n; j++) a[r * n + j] -= f * a[col * n + j];
        }
    }
    free(a);
    return (swaps % 2) ? -det : det;
}

int wubusolver_inv(const double *A, int n, double *inv) {
    if (!A || !inv || n < 1) return -1;
    double *aug = (double *)malloc((size_t)n * (2 * n) * sizeof(double));
    if (!aug) return -1;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) aug[i * (2 * n) + j] = A[i * n + j];
        for (int j = 0; j < n; j++) aug[i * (2 * n) + n + j] = (i == j) ? 1.0 : 0.0;
    }
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabs(aug[r * (2 * n) + col]) > fabs(aug[piv * (2 * n) + col])) piv = r;
        if (fabs(aug[piv * (2 * n) + col]) < 1e-12) { free(aug); return -1; }
        if (piv != col) for (int j = 0; j < 2 * n; j++) {
            double t = aug[col * (2 * n) + j]; aug[col * (2 * n) + j] = aug[piv * (2 * n) + j]; aug[piv * (2 * n) + j] = t;
        }
        double d = aug[col * (2 * n) + col];
        for (int j = 0; j < 2 * n; j++) aug[col * (2 * n) + j] /= d;
        for (int r = 0; r < n; r++) {
            if (r == col) continue;
            double f = aug[r * (2 * n) + col];
            if (f != 0) for (int j = 0; j < 2 * n; j++) aug[r * (2 * n) + j] -= f * aug[col * (2 * n) + j];
        }
    }
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) inv[i * n + j] = aug[i * (2 * n) + n + j];
    free(aug);
    return 0;
}

int wubusolver_solve2(double a11, double a12, double a21, double a22,
                      double b1, double b2, double *x, double *y) {
    double A[4] = {a11, a12, a21, a22}, b[2] = {b1, b2}, sol[2];
    if (wubusolver_solve(A, 2, b, sol) != 0) return -1;
    if (x) *x = sol[0];
    if (y) *y = sol[1];
    return 0;
}
