/* WuBuOffice -- wubuformula/functions/math
 * Aggregate and numeric functions: SUM, PRODUCT, AVERAGE, MIN, MAX, COUNT,
 * COUNTA, COUNTBLANK, ROUND, ROUNDUP, ROUNDDOWN, ABS, INT, MOD, POWER, SQRT,
 * CEILING, FLOOR, PI, SIGN, EXP, LN, LOG10, SUMPRODUCT.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void f_sum(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges;
    wubuval_set_num(out, wubu_sum_flat(flat, fn, NULL));
}
static void f_product(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges;
    double p = 1; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = wubu_to_num(&flat[i], &ok); if (ok) { p *= d; c++; } }
    wubuval_set_num(out, c ? p : 0.0);
}
static void f_average(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges;
    int c; double s = wubu_sum_flat(flat, fn, &c);
    wubuval_set_num(out, c ? s / c : 0.0);
}
static void f_min(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = wubu_to_num(&flat[i], &ok); if (ok) { if (!c || d < m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_max(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = wubu_to_num(&flat[i], &ok); if (ok) { if (!c || d > m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_count(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_NUM) c++;
    wubuval_set_num(out, (double)c);
}
static void f_counta(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind != WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}
static void f_countblank(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)ranges; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}

static void f_round(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f + 0.5) / f);
}
static void f_roundup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, ceil(x * f) / f);
}
static void f_rounddown(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f) / f);
}
static void f_abs(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, fabs(x));
}
static void f_int(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, floor(x));
}
static void f_mod(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1, ok2; double x = wubu_to_num(&a[0], &ok1), y = wubu_to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || y == 0) { wubuval_set_err(out, y == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, fmod(x, y));
}
static void f_power(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1, ok2; double x = wubu_to_num(&a[0], &ok1), y = wubu_to_num(&a[1], &ok2);
    if (!ok1 || !ok2) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, pow(x, y));
}
static void f_sqrt(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok);
    if (!ok || x < 0) { wubuval_set_err(out, x < 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, sqrt(x));
}
static void f_ceiling(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1, ok2; double x = wubu_to_num(&a[0], &ok1), m = wubu_to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, ceil(x / m) * m);
}
static void f_floor(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1, ok2; double x = wubu_to_num(&a[0], &ok1), m = wubu_to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, floor(x / m) * m);
}
static void f_pi(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)flat; (void)fn; (void)ranges; wubuval_set_num(out, M_PI);
}
static void f_sign(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, x > 0 ? 1 : (x < 0 ? -1 : 0));
}
static void f_exp(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, exp(x));
}
static void f_ln(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok);
    if (!ok || x <= 0) { wubuval_set_err(out, x <= 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, log(x));
}
static void f_log10(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok);
    if (!ok || x <= 0) { wubuval_set_err(out, x <= 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, log10(x));
}

/* SUMPRODUCT: pairwise product of corresponding elements across equal-sized
 * ranges, summed. For scalar args, behaves like PRODUCT-of-sum. We require all
 * range args to share (rows*cols); scalars broadcast. */
static void f_sumproduct(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)flat; (void)fn;
    int total = -1;
    for (int i = 0; i < na; i++) {
        const wubu_func_range *r = &ranges[i];
        int n = (r->cols > 0) ? r->rows * r->cols : 1;
        if (total < 0) total = n;
        else if (n != total) { wubuval_set_err(out, WERR_VALUE); return; }
    }
    if (total < 1) { wubuval_set_num(out, 0.0); return; }
    double sum = 0;
    for (int k = 0; k < total; k++) {
        double prod = 1; int any = 0;
        for (int i = 0; i < na; i++) {
            const wubu_func_range *r = &ranges[i];
            const wubuval *v = (r->cols > 0) ? &r->grid[k] : &a[i];
            int ok; double d = wubu_to_num(v, &ok);
            if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
            prod *= d; any = 1;
        }
        if (any) sum += prod;
    }
    wubuval_set_num(out, sum);
}

void wubu_register_math(wubu_func_registrar reg) {
    reg("SUM", f_sum);
    reg("PRODUCT", f_product);
    reg("AVERAGE", f_average);
    reg("MIN", f_min);
    reg("MAX", f_max);
    reg("COUNT", f_count);
    reg("COUNTA", f_counta);
    reg("COUNTBLANK", f_countblank);
    reg("ROUND", f_round);
    reg("ROUNDUP", f_roundup);
    reg("ROUNDDOWN", f_rounddown);
    reg("ABS", f_abs);
    reg("INT", f_int);
    reg("MOD", f_mod);
    reg("POWER", f_power);
    reg("SQRT", f_sqrt);
    reg("CEILING", f_ceiling);
    reg("FLOOR", f_floor);
    reg("PI", f_pi);
    reg("SIGN", f_sign);
    reg("EXP", f_exp);
    reg("LN", f_ln);
    reg("LOG10", f_log10);
    reg("SUMPRODUCT", f_sumproduct);
}
