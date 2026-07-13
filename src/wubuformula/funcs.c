/* WuBuOffice -- wubuformula/funcs
 * Built-in function library (arithmetic, logic, text, math, statistics).
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "funcs.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int strcasecmp_local(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* ---- coercion helpers ---- */
static double to_num(const wubuval *v, int *ok) {
    *ok = 1;
    switch (v->kind) {
        case WV_NUM:   return v->num;
        case WV_BOOL:  return v->boolean ? 1.0 : 0.0;
        case WV_EMPTY: return 0.0;
        case WV_STR: {
            /* try to parse a number from the string */
            char *end; double d = strtod(v->str, &end);
            if (end == v->str || *end != '\0') { *ok = 0; return 0.0; }
            return d;
        }
        default: *ok = 0; return 0.0;
    }
}
static int to_bool(const wubuval *v) {
    switch (v->kind) {
        case WV_BOOL:  return v->boolean;
        case WV_NUM:   return v->num != 0.0;
        case WV_STR:   return v->str && v->str[0] != '\0' && strcasecmp_local(v->str, "FALSE") != 0;
        default:       return 0;
    }
}
static const char *to_str(const wubuval *v) {
    switch (v->kind) {
        case WV_STR:   return v->str ? v->str : "";
        case WV_NUM:   return NULL; /* caller renders */
        case WV_BOOL:  return v->boolean ? "TRUE" : "FALSE";
        default:       return "";
    }
}
/* ---- aggregate helpers ---- */
static double sum_flat(const wubuval *flat, int n, int *cnt) {
    double s = 0; int c = 0;
    for (int i = 0; i < n; i++) {
        int ok; double d = to_num(&flat[i], &ok);
        if (ok) { s += d; c++; }
    }
    if (cnt) *cnt = c;
    return s;
}

/* ================= functions ================= */

static void f_sum(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; wubuval_set_num(out, sum_flat(flat, fn, NULL));
}
static void f_product(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na;
    double p = 1; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { p *= d; c++; } }
    wubuval_set_num(out, c ? p : 0.0);
}
static void f_average(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; int c; double s = sum_flat(flat, fn, &c);
    wubuval_set_num(out, c ? s / c : 0.0);
}
static void f_min(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { if (!c || d < m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_max(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { if (!c || d > m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_count(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_NUM) c++;
    wubuval_set_num(out, (double)c);
}
static void f_counta(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind != WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}
static void f_countblank(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}

static void f_if(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    int cond = to_bool(&a[0]);
    if (cond) { if (na >= 2) wubuval_copy(out, &a[1]); else wubuval_set_bool(out, 1); }
    else { if (na >= 3) wubuval_copy(out, &a[2]); else wubuval_set_bool(out, 0); }
}
static void f_iferror(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (a[0].kind == WV_ERR) wubuval_copy(out, &a[1]);
    else wubuval_copy(out, &a[0]);
}
static void f_and(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; int r = 1;
    for (int i = 0; i < fn; i++) if (!to_bool(&flat[i])) { r = 0; break; }
    wubuval_set_bool(out, r);
}
static void f_or(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; int r = 0;
    for (int i = 0; i < fn; i++) if (to_bool(&flat[i])) { r = 1; break; }
    wubuval_set_bool(out, r);
}
static void f_not(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)na; (void)flat; (void)fn;
    wubuval_set_bool(out, !to_bool(&a[0]));
}

static void f_round(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f + 0.5) / f);
}
static void f_roundup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, ceil(x * f) / f);
}
static void f_rounddown(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f) / f);
}
static void f_abs(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, fabs(x));
}
static void f_int(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, floor(x));
}
static void f_mod(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), y = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || y == 0) { wubuval_set_err(out, y == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, fmod(x, y));
}
static void f_power(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), y = to_num(&a[1], &ok2);
    if (!ok1 || !ok2) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, pow(x, y));
}
static void f_sqrt(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok || x < 0) { wubuval_set_err(out, x < 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, sqrt(x));
}
static void f_ceiling(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), m = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, ceil(x / m) * m);
}
static void f_floor(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), m = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, floor(x / m) * m);
}
static void f_pi(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a; (void)na; (void)flat; (void)fn; wubuval_set_num(out, M_PI);
}
static void f_sign(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, x > 0 ? 1 : (x < 0 ? -1 : 0));
}
static void f_exp(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, exp(x));
}
static void f_ln(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok || x <= 0) { wubuval_set_err(out, x <= 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, log(x));
}
static void f_log10(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok || x <= 0) { wubuval_set_err(out, x <= 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, log10(x));
}

/* ---- text ---- */
static char *num_to_str(double x) {
    char buf[64];
    if (x == (long long)x && fabs(x) < 1e15) snprintf(buf, sizeof buf, "%.0f", x);
    else snprintf(buf, sizeof buf, "%.12g", x);
    return strdup(buf);
}
static void f_concatenate(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;
    size_t cap = 64, n = 0; char *s = malloc(cap); s[0] = '\0';
    for (int i = 0; i < fn; i++) {
        const wubuval *v = &flat[i];
        const char *t;
        char *owned = NULL;
        if (v->kind == WV_STR) t = v->str ? v->str : "";
        else if (v->kind == WV_NUM) { owned = num_to_str(v->num); t = owned; }
        else if (v->kind == WV_BOOL) t = v->boolean ? "TRUE" : "FALSE";
        else t = "";
        size_t tl = strlen(t);
        if (n + tl + 1 >= cap) { cap = n + tl + 1 + 64; s = realloc(s, cap); }
        memcpy(s + n, t, tl); n += tl; s[n] = '\0';
        free(owned);
    }
    wubuval_set_str(out, s);
    free(s);
}
static void f_left(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int n = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 1;
    if (n < 0) n = 0;
    char *s = malloc((size_t)n + 1); memcpy(s, t, (size_t)n); s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_right(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int L = (int)strlen(t);
    int n = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 1;
    if (n < 0) n = 0;
    int start = L - n; if (start < 0) start = 0;
    wubuval_set_str(out, t + start);
    free(owned);
}
static void f_mid(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int start = (int)to_num(&a[1], &(int){0}); /* 1-based */
    int len = (na >= 3) ? (int)to_num(&a[2], &(int){0}) : (int)strlen(t);
    if (start < 1) start = 1;
    int from = start - 1; if (from < 0) from = 0;
    if (len < 0) len = 0;
    int L = (int)strlen(t);
    if (from > L) from = L;
    int avail = L - from; if (len > avail) len = avail;
    char *s = malloc((size_t)len + 1); memcpy(s, t + from, (size_t)len); s[len] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_len(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    wubuval_set_num(out, (double)strlen(t ? t : ""));
    free(owned);
}
static void f_upper(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)toupper((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_lower(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)tolower((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_trim(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t); char *s = malloc(L + 1); size_t n = 0; int sp = 1;
    for (size_t i = 0; i < L; i++) {
        if (isspace((unsigned char)t[i])) { if (!sp && i + 1 < L) s[n++] = ' '; sp = 1; }
        else { s[n++] = t[i]; sp = 0; }
    }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_text(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* simplified: ignore format, render number/value as text */
    if (a[0].kind == WV_STR) wubuval_set_str(out, a[0].str);
    else if (a[0].kind == WV_NUM) { char *s = num_to_str(a[0].num); wubuval_set_str(out, s); free(s); }
    else if (a[0].kind == WV_BOOL) wubuval_set_str(out, a[0].boolean ? "TRUE" : "FALSE");
    else wubuval_set_str(out, "");
}
static void f_value(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, x);
}
static void f_rept(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int times = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    if (times < 0) times = 0;
    size_t L = strlen(t); size_t cap = L * (size_t)times + 1;
    char *s = malloc(cap); size_t n = 0;
    for (int i = 0; i < times; i++) { memcpy(s + n, t, L); n += L; }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_exact(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *x = to_str(&a[0]); const char *y = to_str(&a[1]);
    char *ox = NULL, *oy = NULL;
    if (!x && a[0].kind == WV_NUM) { ox = num_to_str(a[0].num); x = ox; }
    if (!y && a[1].kind == WV_NUM) { oy = num_to_str(a[1].num); y = oy; }
    wubuval_set_bool(out, strcmp(x ? x : "", y ? y : "") == 0);
    free(ox); free(oy);
}

/* ---- lookups over a real range (arg index 1 must be a RANGE) ---- */
static void f_vlookup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* VLOOKUP(key, table, col_index[, range_lookup]) — search first column of
     * the table range for key, return value from col_index (1-based). We do an
     * exact (non-approximate) match. */
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int ok; double key = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int col = (int)to_num(&a[2], &(int){0}); if (col < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    if (col > t->cols) { wubuval_set_err(out, WERR_REF); return; }
    for (int r = 0; r < t->rows; r++) {
        const wubuval *c0 = &t->grid[r * t->cols];
        int ok2; double d = to_num(c0, &ok2);
        if (ok2 && d == key) { wubuval_copy(out, &t->grid[r * t->cols + (col - 1)]); return; }
    }
    wubuval_set_err(out, WERR_NA);
}
static void f_hlookup(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* HLOOKUP(key, table, row_index) — search first ROW of the table for key. */
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int ok; double key = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int row = (int)to_num(&a[2], &(int){0}); if (row < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    if (row > t->rows) { wubuval_set_err(out, WERR_REF); return; }
    for (int c = 0; c < t->cols; c++) {
        const wubuval *c0 = &t->grid[c];
        int ok2; double d = to_num(c0, &ok2);
        if (ok2 && d == key) { wubuval_copy(out, &t->grid[(row - 1) * t->cols + c]); return; }
    }
    wubuval_set_err(out, WERR_NA);
}

/* condition match: a[1] is the criteria expression; we support
 *   number  -> equal to number
 *   ">n" "<n" ">=n" "<=n" "<>n" "=n"  -> numeric comparisons
 *   "txt"  -> exact string equal (case-insensitive)
 * Returns 1 if `v` satisfies the criterion. */
static int match_criteria(const wubuval *v, const wubuval *crit) {
    if (crit->kind == WV_NUM) {
        int ok; double x = to_num(v, &ok); return ok && x == crit->num;
    }
    if (crit->kind == WV_STR && crit->str) {
        const char *s = crit->str;
        if (s[0] == '>' || s[0] == '<' || s[0] == '=') {
            int cmp = 0; const char *p = s;
            if (s[0] == '<' && s[1] == '>') { cmp = 2; p = s + 2; }
            else if (s[0] == '>' && s[1] == '=') { cmp = 3; p = s + 2; }
            else if (s[0] == '<' && s[1] == '=') { cmp = 4; p = s + 2; }
            else if (s[0] == '>') { cmp = 1; p = s + 1; }
            else if (s[0] == '<') { cmp = 5; p = s + 1; }
            else if (s[0] == '=') { cmp = 0; p = s + 1; }
            char *end; double thr = strtod(p, &end);
            if (end == p) { /* not a number: string compare on '=' only */
                return cmp == 0 && strcasecmp_local(s + 1, v->kind==WV_STR?v->str:"") == 0;
            }
            int ok; double x = to_num(v, &ok); if (!ok) return 0;
            switch (cmp) {
                case 0: return x == thr;
                case 1: return x > thr;
                case 2: return x != thr;
                case 3: return x >= thr;
                case 4: return x <= thr;
                case 5: return x < thr;
                default: return 0;
            }
        }
        /* plain string: exact, case-insensitive */
        if (v->kind == WV_STR) return strcasecmp_local(v->str ? v->str : "", s) == 0;
        if (v->kind == WV_NUM) { char *xs = num_to_str(v->num); int r = strcasecmp_local(xs, s) == 0; free(xs); return r; }
        return 0;
    }
    return 0;
}
static void f_sumif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* SUMIF(range, criteria [, sum_range]) */
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *sr = (na >= 3) ? &ranges[2] : cr;
    double sum = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) {
        if (match_criteria(&cr->grid[i], &a[1])) {
            int ri = i / cr->cols, ci = i % cr->cols;
            const wubuval *sv = (na >= 3 && sr->grid) ? &sr->grid[ri * sr->cols + ci] : &cr->grid[i];
            int ok; double d = to_num(sv, &ok); if (ok) sum += d;
        }
    }
    wubuval_set_num(out, sum);
}
static void f_countif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int c = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) if (match_criteria(&cr->grid[i], &a[1])) c++;
    wubuval_set_num(out, (double)c);
}

/* ---- text ---- */
static void f_substitute(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    const char *old = (a[1].kind == WV_STR) ? a[1].str : "";
    const char *neu = (na >= 3 && a[2].kind == WV_STR) ? a[2].str : (na >= 3 ? "" : "");
    size_t L = strlen(t ? t : ""), oL = strlen(old), nL = strlen(neu);
    size_t cap = L + 1; char *s = malloc(cap); size_t n = 0;
    const char *p = t;
    size_t which = (na >= 4) ? (size_t)to_num(&a[3], &(int){0}) : 0;
    size_t seen = 0;
    while (*p) {
        if (oL && strncmp(p, old, oL) == 0) {
            seen++;
            if (which == 0 || which == seen) {
                if (n + nL + 1 > cap) { cap = n + nL + 1 + L; s = realloc(s, cap); }
                memcpy(s + n, neu, nL); n += nL; p += oL; continue;
            }
        }
        if (n + 2 > cap) { cap = n + L + 1; s = realloc(s, cap); }
        s[n++] = *p++;
    }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static int str_find(const char *hay, const char *needle, int icase) {
    if (!hay || !needle || !*needle) return 1; /* found at position 1 */
    size_t hl = strlen(hay), nl = strlen(needle);
    for (size_t i = 0; i + nl <= hl; i++) {
        int eq = icase
            ? (strncasecmp(hay + i, needle, nl) == 0)
            : (strncmp(hay + i, needle, nl) == 0);
        if (eq) return (int)(i + 1); /* 1-based */
    }
    return 0;
}
static void f_find(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* FIND(find_text, within_text [, start]) — case-sensitive */
    const char *nd = (a[0].kind==WV_STR)?a[0].str:"";
    const char *t = to_str(&a[1]); char *ot = NULL; if (!t && a[1].kind==WV_NUM){ot=num_to_str(a[1].num);t=ot;}
    int pos = str_find(t ? t : "", nd, 0);
    if (pos == 0) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, pos);
    free(ot);
}
static void f_search(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    /* SEARCH(find_text, within_text [, start]) — case-insensitive */
    const char *nd = (a[0].kind==WV_STR)?a[0].str:"";
    const char *t = to_str(&a[1]); char *ot = NULL; if (!t && a[1].kind==WV_NUM){ot=num_to_str(a[1].num);t=ot;}
    int pos = str_find(t ? t : "", nd, 1);
    if (pos == 0) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, pos);
    free(ot);
}
static void f_proper(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t ? t : ""); char *s = malloc(L + 1);
    int wordstart = 1;
    for (size_t i = 0; i < L; i++) {
        char c = t[i];
        if (isspace((unsigned char)c) || ispunct((unsigned char)c)) { s[i] = c; wordstart = 1; }
        else if (wordstart) { s[i] = (char)toupper((unsigned char)c); wordstart = 0; }
        else s[i] = (char)tolower((unsigned char)c);
    }
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_replace(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int start = (int)to_num(&a[1], &(int){0}); /* 1-based */
    int len = (na >= 3) ? (int)to_num(&a[2], &(int){0}) : 0;
    const char *neu = (na >= 4 && a[3].kind == WV_STR) ? a[3].str : "";
    size_t L = strlen(t ? t : "");
    int from = start - 1; if (from < 0) from = 0;
    if (len < 0) len = 0;
    if ((size_t)from > L) from = (int)L;
    size_t nL = strlen(neu);
    size_t cap = L + nL + 1; char *s = malloc(cap); size_t n = 0;
    memcpy(s, t, (size_t)from); n = (size_t)from;
    memcpy(s + n, neu, nL); n += nL;
    if ((size_t)(from + len) < L) { memcpy(s + n, t + from + len, L - (size_t)(from + len)); n += L - (size_t)(from + len); }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}

/* ---- date & time (Excel serial: days since 1899-12-30) ---- */
/* Excel's "1900 date system" erroneously treats 1900 as a leap year (the
 * Lotus 1-2-3 compatibility bug): serial 60 = the phantom 1900-02-29, and
 * every date from 1900-03-01 onward is shifted +1 vs. the true Gregorian
 * calendar. We compute the pure-Gregorian serial then apply that +1 shift. */
static long excel_serial(int y, int m, int d) {
    if (m < 1 || m > 12 || d < 1) return 0;
    if (y == 1900 && m == 2 && d == 29) return 60; /* the phantom leap day */
    if (y == 1900 && m == 2 && d > 29) d = 29;      /* clamp impossible day */
    /* pure-Gregorian Julian Day Number (Fliegel & Van Flandern) */
    int a = (14 - m) / 12;
    int yy = y + 4800 - a;
    int mm = m + 12 * a - 3;
    long jdn = d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
    long greg = jdn - 2415020L; /* 1900-01-01 -> serial 1 */
    if (greg >= 60) greg += 1;  /* Excel's 1900 leap bug shift */
    return greg;
}
static void serial_to_ymd(long s, int *y, int *m, int *d) {
    long g = (s >= 61) ? s - 1 : s; /* undo the leap-bug shift */
    long jdn = g + 2415020L;
    /* invert JDN to Y/M/D (Fliegel & Van Flandern) */
    long a = jdn + 32044;
    long b = (4 * a + 3) / 146097;
    long c = a - (146097 * b) / 4;
    long dd = (4 * c + 3) / 1461;
    long e = c - (1461 * dd) / 4;
    long mm = (5 * e + 2) / 153;
    *d = (int)(e - (153 * mm + 2) / 5 + 1);
    *m = (int)(mm + 3 - 12 * (mm / 10));
    *y = (int)(100 * b + dd - 4800 + (mm / 10));
}
static void f_date(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok1,ok2,ok3; int y=(int)to_num(&a[0],&ok1), m=(int)to_num(&a[1],&ok2), d=(int)to_num(&a[2],&ok3);
    if (!ok1||!ok2||!ok3) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, (double)excel_serial(y, m, d));
}
static void f_year(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)y);
}
static void f_month(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)m);
}
static void f_day(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; long s=(long)to_num(&a[0],&ok); if(!ok){wubuval_set_err(out,WERR_VALUE);return;}
    int y,m,d; serial_to_ymd(s,&y,&m,&d); wubuval_set_num(out,(double)d);
}
static void f_today(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;(void)na;(void)flat;(void)fn;(void)ranges;
    /* current date as serial (no time component) */
    time_t t = time(NULL); struct tm *lt = localtime(&t);
    wubuval_set_num(out, (double)excel_serial(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday));
}
static void f_now(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;(void)na;(void)flat;(void)fn;(void)ranges;
    time_t t = time(NULL); struct tm *lt = localtime(&t);
    double serial = (double)excel_serial(lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    double frac = (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec) / 86400.0;
    wubuval_set_num(out, serial + frac);
}

/* ---- financial ---- */
static void f_pmt(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int o1,o2,o3; double rate=to_num(&a[0],&o1), nper=to_num(&a[1],&o2), pv=to_num(&a[2],&o3);
    if (!o1||!o2||!o3) { wubuval_set_err(out,WERR_VALUE); return; }
    double fv = (na>=4)?to_num(&a[3],&(int){0}):0.0;
    double type = (na>=5)?to_num(&a[4],&(int){0}):0.0;
    double pmt;
    if (rate == 0) pmt = -(pv + fv) / nper;
    else {
        double f = pow(1+rate, nper);
        /* standard closed form: pmt = -(fv*f + pv) / ((1/rate - type)*(1 - 1/f)) */
        pmt = -(fv * f + pv) / ((1.0/rate - type) * (1.0 - 1.0/f));
    }
    wubuval_set_num(out, pmt);
}
static void f_fv(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int o1,o2,o3; double rate=to_num(&a[0],&o1), nper=to_num(&a[1],&o2), pmt=to_num(&a[2],&o3);
    if (!o1||!o2||!o3) { wubuval_set_err(out,WERR_VALUE); return; }
    double pv = (na>=4)?to_num(&a[3],&(int){0}):0.0;
    double type = (na>=5)?to_num(&a[4],&(int){0}):0.0;
    double f = (rate == 0) ? 1.0 : pow(1+rate, nper);
    double fv = -(pv * f + pmt * (1 + rate * type) * (f - 1) / (rate == 0 ? 1 : rate));
    wubuval_set_num(out, fv);
}

/* ---- INDEX / MATCH over a real range ---- */
static void f_index(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    const wubu_func_range *t = &ranges[0];
    if (!t->grid || t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int row = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 1;
    int col = (na >= 3 && na > 2) ? (int)to_num(&a[2], &(int){0}) : 1;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (row < 1 || row > t->rows || col < 1 || col > t->cols) { wubuval_set_err(out, WERR_REF); return; }
    wubuval_copy(out, &t->grid[(row - 1) * t->cols + (col - 1)]);
}
static void f_match(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *t = &ranges[1]; /* second arg is the lookup array */
    if (!t->grid || t->rows * t->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    /* single row or single column expected; we scan row-major and return the
     * 1-based position (row*cols+col in 1-based terms across the flat array). */
    int n = t->rows * t->cols;
    for (int i = 0; i < n; i++) {
        int ok; double d = to_num(&t->grid[i], &ok);
        int match = 0;
        if (ok && a[0].kind == WV_NUM) match = (d == to_num(&a[0], &(int){0}));
        else if (a[0].kind == WV_STR) {
            const char *s = to_str(&a[0]);
            if (t->grid[i].kind == WV_STR) match = (strcasecmp_local(t->grid[i].str ? t->grid[i].str : "", s ? s : "") == 0);
        }
        if (match) { wubuval_set_num(out, (double)(i + 1)); return; }
    }
    wubuval_set_err(out, WERR_NA);
}

/* ---- AVERAGEIF / CHOOSE ---- */
static void f_averageif(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *cr = &ranges[0];
    if (!cr->grid || cr->rows < 1 || cr->cols < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    const wubu_func_range *sr = (na >= 3) ? &ranges[2] : cr;
    double sum = 0; int c = 0;
    for (int i = 0; i < cr->rows * cr->cols; i++) {
        if (match_criteria(&cr->grid[i], &a[1])) {
            int ri = i / cr->cols, ci = i % cr->cols;
            const wubuval *sv = (na >= 3 && sr->grid) ? &sr->grid[ri * sr->cols + ci] : &cr->grid[i];
            int ok; double d = to_num(sv, &ok); if (ok) { sum += d; c++; }
        }
    }
    wubuval_set_num(out, c ? sum / c : 0.0);
}
static void f_choose(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    int idx = (int)to_num(&a[0], &(int){0});
    if (idx < 1 || idx > na - 1) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_copy(out, &a[idx]); /* a[0] is index; a[1..] are choices */
}

/* ---- registration ---- */
typedef struct { const char *name; wubu_func_impl fn; } fent;
static const fent TABLE[] = {
    {"SUM", f_sum}, {"PRODUCT", f_product}, {"AVERAGE", f_average}, {"MIN", f_min},
    {"MAX", f_max}, {"COUNT", f_count}, {"COUNTA", f_counta}, {"COUNTBLANK", f_countblank},
    {"IF", f_if}, {"IFERROR", f_iferror}, {"AND", f_and}, {"OR", f_or}, {"NOT", f_not},
    {"ROUND", f_round}, {"ROUNDUP", f_roundup}, {"ROUNDDOWN", f_rounddown}, {"ABS", f_abs},
    {"INT", f_int}, {"MOD", f_mod}, {"POWER", f_power}, {"SQRT", f_sqrt},
    {"CEILING", f_ceiling}, {"FLOOR", f_floor}, {"PI", f_pi}, {"SIGN", f_sign},
    {"EXP", f_exp}, {"LN", f_ln}, {"LOG10", f_log10},
    {"CONCATENATE", f_concatenate}, {"LEFT", f_left}, {"RIGHT", f_right}, {"MID", f_mid},
    {"LEN", f_len}, {"UPPER", f_upper}, {"LOWER", f_lower}, {"TRIM", f_trim},
    {"TEXT", f_text}, {"VALUE", f_value}, {"REPT", f_rept}, {"EXACT", f_exact},
    {"SUBSTITUTE", f_substitute}, {"FIND", f_find}, {"SEARCH", f_search},
    {"PROPER", f_proper}, {"REPLACE", f_replace},
    {"VLOOKUP", f_vlookup}, {"HLOOKUP", f_hlookup},
    {"SUMIF", f_sumif}, {"COUNTIF", f_countif},
    {"INDEX", f_index}, {"MATCH", f_match},
    {"AVERAGEIF", f_averageif}, {"CHOOSE", f_choose},
    {"DATE", f_date}, {"TODAY", f_today}, {"NOW", f_now},
    {"YEAR", f_year}, {"MONTH", f_month}, {"DAY", f_day},
    {"PMT", f_pmt}, {"FV", f_fv},
    {NULL, NULL}
};

wubu_func_impl wubu_func_lookup(const char *name) {
    for (int i = 0; TABLE[i].name; i++)
        if (strcasecmp_local(TABLE[i].name, name) == 0) return TABLE[i].fn;
    return NULL;
}
int wubu_func_count(void) { int n = 0; while (TABLE[n].name) n++; return n; }
