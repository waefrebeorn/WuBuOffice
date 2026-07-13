/* WuBuOffice -- wubuformula/functions/stat
 * Statistical functions: MEDIAN, MODE, STDEV, VAR, LARGE, SMALL. Samples are
 * collected per-argument from the range geometry (ranges[]) so scalar control
 * parameters (e.g. LARGE's k, TEXTJOIN's delimiter) are never folded into the
 * numeric sample. Uses wubu_numeric_flat for the flat fallback.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

static int dcmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* Collect numerics from a single argument (range grid if present, else the
 * scalar). Returns a freshly-allocated array (caller frees) of length *out_n,
 * or NULL if none. */
static double *arg_numeric(const wubuval *arg, const wubu_func_range *r, int *out_n) {
    if (r && r->cols > 0 && r->grid) {
        return wubu_numeric_flat(r->grid, r->rows * r->cols, out_n);
    }
    int ok; double d = wubu_to_num(arg, &ok);
    if (!ok) { *out_n = 0; return NULL; }
    double *v = malloc(sizeof(double));
    v[0] = d; *out_n = 1; return v;
}

/* Collect numerics from ALL arguments (each expanded if a range). Used by
 * MEDIAN/MODE/STDEV/VAR which take a homogeneous value list. */
static double *all_args_numeric(const wubuval *a, int na, const wubu_func_range *ranges, int *out_n) {
    int cap = 16, m = 0; double *v = malloc((size_t)cap * sizeof(double));
    for (int i = 0; i < na; i++) {
        int cn; double *cv = arg_numeric(&a[i], &ranges[i], &cn);
        if (!cv) continue;
        for (int j = 0; j < cn; j++) {
            if (m == cap) { cap *= 2; v = realloc(v, (size_t)cap * sizeof(double)); }
            v[m++] = cv[j];
        }
        free(cv);
    }
    *out_n = m;
    if (!m) { free(v); return NULL; }
    return v;
}

static void f_median(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = all_args_numeric(a, na, ranges, &n);
    if (!v) { wubuval_set_err(out, WERR_NA); return; }
    qsort(v, (size_t)n, sizeof(double), dcmp);
    double m = (n % 2) ? v[n/2] : (v[n/2 - 1] + v[n/2]) / 2.0;
    wubuval_set_num(out, m);
    free(v);
}
static void f_mode(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = all_args_numeric(a, na, ranges, &n);
    if (!v) { wubuval_set_err(out, WERR_NA); return; }
    qsort(v, (size_t)n, sizeof(double), dcmp);
    double best = 0; int bestc = 0;
    int i = 0;
    while (i < n) {
        int j = i; double val = v[i];
        while (j < n && v[j] == val) j++;
        int c = j - i;
        if (c > bestc) { bestc = c; best = val; }
        i = j;
    }
    if (bestc <= 1) { free(v); wubuval_set_err(out, WERR_NA); return; }
    wubuval_set_num(out, best);
    free(v);
}
/* sample standard deviation (n-1) */
static void f_stdev(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = all_args_numeric(a, na, ranges, &n);
    if (!v || n < 2) { free(v); wubuval_set_err(out, WERR_NA); return; }
    double mean = 0; for (int i = 0; i < n; i++) mean += v[i]; mean /= n;
    double s = 0; for (int i = 0; i < n; i++) { double d = v[i] - mean; s += d * d; }
    wubuval_set_num(out, sqrt(s / (n - 1)));
    free(v);
}
/* sample variance (n-1) */
static void f_var(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = all_args_numeric(a, na, ranges, &n);
    if (!v || n < 2) { free(v); wubuval_set_err(out, WERR_NA); return; }
    double mean = 0; for (int i = 0; i < n; i++) mean += v[i]; mean /= n;
    double s = 0; for (int i = 0; i < n; i++) { double d = v[i] - mean; s += d * d; }
    wubuval_set_num(out, s / (n - 1));
    free(v);
}
static void f_large(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = arg_numeric(&a[0], &ranges[0], &n);
    if (!v) { wubuval_set_err(out, WERR_NA); return; }
    int k = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    if (k < 1 || k > n) { free(v); wubuval_set_err(out, WERR_NUM); return; }
    qsort(v, (size_t)n, sizeof(double), dcmp);
    wubuval_set_num(out, v[n - k]); /* k-th largest */
    free(v);
}
static void f_small(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int n; double *v = arg_numeric(&a[0], &ranges[0], &n);
    if (!v) { wubuval_set_err(out, WERR_NA); return; }
    int k = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    if (k < 1 || k > n) { free(v); wubuval_set_err(out, WERR_NUM); return; }
    qsort(v, (size_t)n, sizeof(double), dcmp);
    wubuval_set_num(out, v[k - 1]); /* k-th smallest */
    free(v);
}

void wubu_register_stat(wubu_func_registrar reg) {
    reg("MEDIAN", f_median);
    reg("MODE", f_mode);
    reg("STDEV", f_stdev);
    reg("VAR", f_var);
    reg("LARGE", f_large);
    reg("SMALL", f_small);
}
