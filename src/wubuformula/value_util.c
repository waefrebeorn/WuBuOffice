/* WuBuOffice -- wubuformula/value_util
 * Shared value coercion / comparison helpers (single source of truth for the
 * formula function modules). Clean-room, from-scratch (SLERM). */

#include "value_util.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>

int wubu_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

double wubu_to_num(const wubuval *v, int *ok) {
    *ok = 1;
    switch (v->kind) {
        case WV_NUM:   return v->num;
        case WV_BOOL:  return v->boolean ? 1.0 : 0.0;
        case WV_EMPTY: return 0.0;
        case WV_STR: {
            char *end; double d = strtod(v->str, &end);
            if (end == v->str || *end != '\0') { *ok = 0; return 0.0; }
            return d;
        }
        default: *ok = 0; return 0.0;
    }
}

int wubu_to_bool(const wubuval *v) {
    switch (v->kind) {
        case WV_BOOL:  return v->boolean;
        case WV_NUM:   return v->num != 0.0;
        case WV_STR:   return v->str && v->str[0] != '\0' && wubu_strcasecmp(v->str, "FALSE") != 0;
        default:       return 0;
    }
}

const char *wubu_to_str(const wubuval *v) {
    switch (v->kind) {
        case WV_STR:   return v->str ? v->str : "";
        case WV_NUM:   return NULL; /* caller renders */
        case WV_BOOL:  return v->boolean ? "TRUE" : "FALSE";
        default:       return "";
    }
}

char *wubu_num_to_str(double x) {
    char buf[64];
    if (x == (long long)x && fabs(x) < 1e15) snprintf(buf, sizeof buf, "%.0f", x);
    else snprintf(buf, sizeof buf, "%.12g", x);
    return strdup(buf);
}

double wubu_sum_flat(const wubuval *flat, int n, int *cnt) {
    double s = 0; int c = 0;
    for (int i = 0; i < n; i++) {
        int ok; double d = wubu_to_num(&flat[i], &ok);
        if (ok) { s += d; c++; }
    }
    if (cnt) *cnt = c;
    return s;
}

int wubu_match_criteria(const wubuval *v, const wubuval *crit) {
    if (crit->kind == WV_NUM) {
        int ok; double x = wubu_to_num(v, &ok); return ok && x == crit->num;
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
                return cmp == 0 && wubu_strcasecmp(s + 1, v->kind == WV_STR ? v->str : "") == 0;
            }
            int ok; double x = wubu_to_num(v, &ok); if (!ok) return 0;
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
        if (v->kind == WV_STR) return wubu_strcasecmp(v->str ? v->str : "", s) == 0;
        if (v->kind == WV_NUM) { char *xs = wubu_num_to_str(v->num); int r = wubu_strcasecmp(xs, s) == 0; free(xs); return r; }
        return 0;
    }
    return 0;
}

double *wubu_numeric_flat(const wubuval *flat, int n, int *out_n) {
    int cap = 8, m = 0; double *arr = malloc((size_t)cap * sizeof(double));
    for (int i = 0; i < n; i++) {
        int ok; double d = wubu_to_num(&flat[i], &ok);
        if (!ok) continue;
        if (m == cap) { cap *= 2; arr = realloc(arr, (size_t)cap * sizeof(double)); }
        arr[m++] = d;
    }
    *out_n = m;
    if (!m) { free(arr); return NULL; }
    return arr;
}
