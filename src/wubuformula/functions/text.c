/* WuBuOffice -- wubuformula/functions/text
 * Text functions: CONCATENATE, CONCAT, TEXTJOIN, LEFT, RIGHT, MID, LEN, UPPER,
 * LOWER, TRIM, TEXT, VALUE, REPT, EXACT, SUBSTITUTE, FIND, SEARCH, PROPER,
 * REPLACE.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* 1-based first occurrence of needle in hay (strncasecmp when icase). */
static int str_find(const char *hay, const char *needle, int icase) {
    if (!hay || !needle || !*needle) return 1;
    size_t hl = strlen(hay), nl = strlen(needle);
    for (size_t i = 0; i + nl <= hl; i++) {
        int eq = icase ? (strncasecmp(hay + i, needle, nl) == 0)
                       : (strncmp(hay + i, needle, nl) == 0);
        if (eq) return (int)(i + 1);
    }
    return 0;
}

/* Render any value to a freshly-allocated string (caller frees); numbers use
 * the shared numeric formatter. */
static char *val_to_text(const wubuval *v) {
    switch (v->kind) {
        case WV_STR:   return strdup(v->str ? v->str : "");
        case WV_NUM:   return wubu_num_to_str(v->num);
        case WV_BOOL:  return strdup(v->boolean ? "TRUE" : "FALSE");
        default:       return strdup("");
    }
}

static void f_concatenate(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)a;
    size_t cap = 64, n = 0; char *s = malloc(cap); s[0] = '\0';
    for (int i = 0; i < fn; i++) {
        char *t = val_to_text(&flat[i]);
        size_t tl = strlen(t);
        if (n + tl + 1 >= cap) { cap = n + tl + 1 + 64; s = realloc(s, cap); }
        memcpy(s + n, t, tl); n += tl; s[n] = '\0';
        free(t);
    }
    wubuval_set_str(out, s);
    free(s);
}

static void f_concat(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    /* CONCAT is like CONCATENATE but also accepts ranges (flattened in flat). */
    f_concatenate(a, na, flat, fn, ranges, out);
}

/* TEXTJOIN(delimiter, ignore_empty, value1, [value2, ...])
 * value args may be scalars or ranges. We iterate the value arguments only
 * (a[2..]) so the delimiter/ignore_empty scalars never enter the output. */
static void f_textjoin(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat;
    if (na < 3) { wubuval_set_err(out, WERR_VALUE); return; }
    const char *delim = (a[0].kind == WV_STR) ? a[0].str : "";
    int ignore_empty = wubu_to_bool(&a[1]);
    char *s = malloc(64); size_t cap = 64, n = 0; s[0] = '\0';
    for (int i = 2; i < na; i++) {
        /* expand a range arg into its cells; otherwise treat the scalar */
        const wubuval *vals = &a[i];
        int count = 1;
        if (ranges[i].cols > 0 && ranges[i].grid) { vals = ranges[i].grid; count = ranges[i].rows * ranges[i].cols; }
        for (int j = 0; j < count; j++) {
            const wubuval *v = &vals[j];
            char *t = val_to_text(v);
            int empty = (v->kind == WV_EMPTY) || (v->kind == WV_STR && (!t || !t[0]));
            if (ignore_empty && empty) { free(t); continue; }
            if (n && delim) {
                size_t dl = strlen(delim);
                if (n + dl + 1 >= cap) { cap = n + dl + strlen(t) + 2; s = realloc(s, cap); }
                memcpy(s + n, delim, dl); n += dl; s[n] = '\0';
            }
            size_t tl = strlen(t);
            if (n + tl + 1 >= cap) { cap = n + tl + 1 + 64; s = realloc(s, cap); }
            memcpy(s + n, t, tl); n += tl; s[n] = '\0';
            free(t);
        }
    }
    wubuval_set_str(out, s);
    free(s);
}

static void f_left(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    int nn = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    if (nn < 0) nn = 0;
    size_t L = strlen(t);
    if ((size_t)nn > L) nn = (int)L;
    char *s = malloc((size_t)nn + 1); memcpy(s, t, (size_t)nn); s[nn] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_right(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    int L = (int)strlen(t);
    int nn = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 1;
    if (nn < 0) nn = 0;
    int start = L - nn; if (start < 0) start = 0;
    wubuval_set_str(out, t + start);
    free(t);
}
static void f_mid(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    char *t = val_to_text(&a[0]);
    int start = (int)wubu_to_num(&a[1], &(int){0}); /* 1-based */
    int len = (na >= 3) ? (int)wubu_to_num(&a[2], &(int){0}) : (int)strlen(t);
    if (start < 1) start = 1;
    int from = start - 1; if (from < 0) from = 0;
    if (len < 0) len = 0;
    int L = (int)strlen(t);
    if (from > L) from = L;
    int avail = L - from; if (len > avail) len = avail;
    char *s = malloc((size_t)len + 1); memcpy(s, t + from, (size_t)len); s[len] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_len(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    wubuval_set_num(out, (double)strlen(t));
    free(t);
}
static void f_upper(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)toupper((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_lower(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    size_t L = strlen(t); char *s = malloc(L + 1);
    for (size_t i = 0; i < L; i++) s[i] = (char)tolower((unsigned char)t[i]);
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_trim(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    size_t L = strlen(t); char *s = malloc(L + 1); size_t n = 0; int sp = 1;
    for (size_t i = 0; i < L; i++) {
        if (isspace((unsigned char)t[i])) { if (!sp && i + 1 < L) s[n++] = ' '; sp = 1; }
        else { s[n++] = t[i]; sp = 0; }
    }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_text(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *s = val_to_text(&a[0]); /* format string in a[1] ignored for now */
    wubuval_set_str(out, s);
    free(s);
}
static void f_value(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    int ok; double x = wubu_to_num(&a[0], &ok);
    if (!ok) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, x);
}
static void f_rept(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    int times = (na >= 2) ? (int)wubu_to_num(&a[1], &(int){0}) : 0;
    if (times < 0) times = 0;
    size_t L = strlen(t); size_t cap = L * (size_t)times + 1;
    char *s = malloc(cap); size_t n = 0;
    for (int i = 0; i < times; i++) { memcpy(s + n, t, L); n += L; }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_exact(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    char *x = val_to_text(&a[0]); char *y = val_to_text(&a[1]);
    wubuval_set_bool(out, strcmp(x, y) == 0);
    free(x); free(y);
}
static void f_substitute(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    const char *old = (a[1].kind == WV_STR) ? a[1].str : "";
    const char *neu = (na >= 3 && a[2].kind == WV_STR) ? a[2].str : "";
    size_t L = strlen(t), oL = strlen(old), nL = strlen(neu);
    size_t cap = L + 1; char *s = malloc(cap); size_t n = 0;
    const char *p = t;
    size_t which = (na >= 4) ? (size_t)wubu_to_num(&a[3], &(int){0}) : 0;
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
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_find(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const char *nd = (a[0].kind == WV_STR) ? a[0].str : "";
    char *t = val_to_text(&a[1]);
    int pos = str_find(t, nd, 0);
    if (pos == 0) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, pos);
    free(t);
}
static void f_search(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    const char *nd = (a[0].kind == WV_STR) ? a[0].str : "";
    char *t = val_to_text(&a[1]);
    int pos = str_find(t, nd, 1);
    if (pos == 0) wubuval_set_err(out, WERR_VALUE); else wubuval_set_num(out, pos);
    free(t);
}
static void f_proper(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    char *t = val_to_text(&a[0]);
    size_t L = strlen(t); char *s = malloc(L + 1);
    int wordstart = 1;
    for (size_t i = 0; i < L; i++) {
        char c = t[i];
        if (isspace((unsigned char)c) || ispunct((unsigned char)c)) { s[i] = c; wordstart = 1; }
        else if (wordstart) { s[i] = (char)toupper((unsigned char)c); wordstart = 0; }
        else s[i] = (char)tolower((unsigned char)c);
    }
    s[L] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}
static void f_replace(const wubuval *a, int na, const wubuval *flat, int fn, const wubu_func_range *ranges, wubuval *out) {
    (void)flat; (void)fn; (void)ranges;
    if (na < 2) { wubuval_set_err(out, WERR_VALUE); return; }
    char *t = val_to_text(&a[0]);
    int start = (int)wubu_to_num(&a[1], &(int){0}); /* 1-based */
    int len = (na >= 3) ? (int)wubu_to_num(&a[2], &(int){0}) : 0;
    const char *neu = (na >= 4 && a[3].kind == WV_STR) ? a[3].str : "";
    size_t L = strlen(t);
    int from = start - 1; if (from < 0) from = 0;
    if (len < 0) len = 0;
    if ((size_t)from > L) from = (int)L;
    size_t nL = strlen(neu);
    size_t cap = L + nL + 1; char *s = malloc(cap); size_t n = 0;
    memcpy(s, t, (size_t)from); n = (size_t)from;
    memcpy(s + n, neu, nL); n += nL;
    if ((size_t)(from + len) < L) { memcpy(s + n, t + from + len, L - (size_t)(from + len)); n += L - (size_t)(from + len); }
    s[n] = '\0';
    wubuval_set_str(out, s); free(s); free(t);
}

void wubu_register_text(wubu_func_registrar reg) {
    reg("CONCATENATE", f_concatenate);
    reg("CONCAT", f_concat);
    reg("TEXTJOIN", f_textjoin);
    reg("LEFT", f_left);
    reg("RIGHT", f_right);
    reg("MID", f_mid);
    reg("LEN", f_len);
    reg("UPPER", f_upper);
    reg("LOWER", f_lower);
    reg("TRIM", f_trim);
    reg("TEXT", f_text);
    reg("VALUE", f_value);
    reg("REPT", f_rept);
    reg("EXACT", f_exact);
    reg("SUBSTITUTE", f_substitute);
    reg("FIND", f_find);
    reg("SEARCH", f_search);
    reg("PROPER", f_proper);
    reg("REPLACE", f_replace);
}
