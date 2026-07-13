/* WuBuOffice -- wubuformula/funcs
 * Built-in function library (arithmetic, logic, text, math, statistics).
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "funcs.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>

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

static void f_sum(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; wubuval_set_num(out, sum_flat(flat, fn, NULL));
}
static void f_product(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na;
    double p = 1; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { p *= d; c++; } }
    wubuval_set_num(out, c ? p : 0.0);
}
static void f_average(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; int c; double s = sum_flat(flat, fn, &c);
    wubuval_set_num(out, c ? s / c : 0.0);
}
static void f_min(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { if (!c || d < m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_max(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; double m = 0; int c = 0;
    for (int i = 0; i < fn; i++) { int ok; double d = to_num(&flat[i], &ok); if (ok) { if (!c || d > m) m = d; c++; } }
    wubuval_set_num(out, c ? m : 0.0);
}
static void f_count(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_NUM) c++;
    wubuval_set_num(out, (double)c);
}
static void f_counta(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind != WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}
static void f_countblank(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; int c = 0;
    for (int i = 0; i < fn; i++) if (flat[i].kind == WV_EMPTY) c++;
    wubuval_set_num(out, (double)c);
}

static void f_if(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    int cond = to_bool(&a[0]);
    if (cond) { if (na >= 2) wubuval_copy(out, &a[1]); else wubuval_set_bool(out, 1); }
    else { if (na >= 3) wubuval_copy(out, &a[2]); else wubuval_set_bool(out, 0); }
}
static void f_iferror(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    if (a[0].kind == WV_ERR) wubuval_copy(out, &a[1]);
    else wubuval_copy(out, &a[0]);
}
static void f_and(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; int r = 1;
    for (int i = 0; i < fn; i++) if (!to_bool(&flat[i])) { r = 0; break; }
    wubuval_set_bool(out, r);
}
static void f_or(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; int r = 0;
    for (int i = 0; i < fn; i++) if (to_bool(&flat[i])) { r = 1; break; }
    wubuval_set_bool(out, r);
}
static void f_not(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)na; (void)flat; (void)fn;
    wubuval_set_bool(out, !to_bool(&a[0]));
}

static void f_round(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f + 0.5) / f);
}
static void f_roundup(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, ceil(x * f) / f);
}
static void f_rounddown(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int d = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 0;
    double f = pow(10.0, (double)d);
    wubuval_set_num(out, floor(x * f) / f);
}
static void f_abs(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, fabs(x));
}
static void f_int(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, floor(x));
}
static void f_mod(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), y = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || y == 0) { wubuval_set_err(out, y == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, fmod(x, y));
}
static void f_power(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), y = to_num(&a[1], &ok2);
    if (!ok1 || !ok2) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, pow(x, y));
}
static void f_sqrt(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok || x < 0) { wubuval_set_err(out, x < 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, sqrt(x));
}
static void f_ceiling(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), m = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, ceil(x / m) * m);
}
static void f_floor(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok1, ok2; double x = to_num(&a[0], &ok1), m = to_num(&a[1], &ok2);
    if (!ok1 || !ok2 || m == 0) { wubuval_set_err(out, m == 0 ? WERR_DIV0 : WERR_VALUE); return; }
    wubuval_set_num(out, floor(x / m) * m);
}
static void f_pi(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)a; (void)na; (void)flat; (void)fn; wubuval_set_num(out, M_PI);
}
static void f_sign(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, x > 0 ? 1 : (x < 0 ? -1 : 0));
}
static void f_exp(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    wubuval_set_num(out, exp(x));
}
static void f_ln(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok || x <= 0) { wubuval_set_err(out, x <= 0 ? WERR_NUM : WERR_VALUE); return; }
    wubuval_set_num(out, log(x));
}
static void f_log10(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_concatenate(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_left(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    int n = (na >= 2) ? (int)to_num(&a[1], &(int){0}) : 1;
    if (n < 0) n = 0;
    char *s = malloc((size_t)n + 1); memcpy(s, t, (size_t)n); s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_right(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_mid(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_len(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    wubuval_set_num(out, (double)strlen(t ? t : ""));
    free(owned);
}
static void f_upper(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)toupper((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_lower(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    const char *t = to_str(&a[0]); char *owned = NULL;
    if (!t && a[0].kind == WV_NUM) { owned = num_to_str(a[0].num); t = owned; }
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)tolower((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(owned);
}
static void f_trim(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_text(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    /* simplified: ignore format, render number/value as text */
    if (a[0].kind == WV_STR) wubuval_set_str(out, a[0].str);
    else if (a[0].kind == WV_NUM) { char *s = num_to_str(a[0].num); wubuval_set_str(out, s); free(s); }
    else if (a[0].kind == WV_BOOL) wubuval_set_str(out, a[0].boolean ? "TRUE" : "FALSE");
    else wubuval_set_str(out, "");
}
static void f_value(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    int ok; double x = to_num(&a[0], &ok);
    if (!ok) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, x);
}
static void f_rept(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
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
static void f_exact(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)flat; (void)fn;
    const char *x = to_str(&a[0]); const char *y = to_str(&a[1]);
    char *ox = NULL, *oy = NULL;
    if (!x && a[0].kind == WV_NUM) { ox = num_to_str(a[0].num); x = ox; }
    if (!y && a[1].kind == WV_NUM) { oy = num_to_str(a[1].num); y = oy; }
    wubuval_set_bool(out, strcmp(x ? x : "", y ? y : "") == 0);
    free(ox); free(oy);
}

/* ---- lookup (simple VLOOKUP over flat pairs) ---- */
static void f_vlookup(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) {
    (void)fn;
    /* simplified VLOOKUP(key, table, col): table is flat [r1c1,r1c2,...]; we
     * scan rows, compare col1 to key, return requested column. */
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    int ok; double key = to_num(&a[0], &ok); if (!ok) { wubuval_set_err(out, WERR_VALUE); return; }
    int col = (int)to_num(&a[2], &(int){0}); if (col < 1) { wubuval_set_err(out, WERR_VALUE); return; }
    int ncols = (na >= 4) ? (int)to_num(&a[3], &(int){0}) : 2;
    if (ncols < 1) ncols = 2;
    int rows = fn / ncols;
    for (int r = 0; r < rows; r++) {
        const wubuval *c0 = &flat[r * ncols];
        int ok2; double d = to_num(c0, &ok2);
        if (ok2 && d == key) {
            if (col <= ncols) { wubuval_copy(out, &flat[r * ncols + (col - 1)]); return; }
            wubuval_set_err(out, WERR_REF); return;
        }
    }
    wubuval_set_err(out, WERR_NA);
}

static void f_sqrt_null(const wubuval *a, int na, const wubuval *flat, int fn, wubuval *out) { (void)a;(void)na;(void)flat;(void)fn; wubuval_set_err(out, WERR_NA); }

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
    {"VLOOKUP", f_vlookup},
    {NULL, NULL}
};

wubu_func_impl wubu_func_lookup(const char *name) {
    for (int i = 0; TABLE[i].name; i++)
        if (strcasecmp_local(TABLE[i].name, name) == 0) return TABLE[i].fn;
    (void)f_sqrt_null;
    return NULL;
}
int wubu_func_count(void) { int n = 0; while (TABLE[n].name) n++; return n; }
