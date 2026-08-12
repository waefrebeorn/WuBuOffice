#if !defined(_POSIX_C_SOURCE) && !defined(_GNU_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "wubudiff.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* split text into lines (own allocations; caller frees each + the array).
 * A trailing newline does NOT produce an empty final line. */
static char **split_lines(const char *txt, int *n) {
    int cap = 16, cnt = 0;
    char **lines = (char **)malloc(cap * sizeof(char *));
    if (!lines) return NULL;
    /* mutable copy for scanning */
    char *copy = strdup(txt);
    if (!copy) { free(lines); return NULL; }
    size_t len = strlen(copy);
    /* trim one trailing newline so "a\nb\n" -> ["a","b"] not ["a","b",""] */
    if (len > 0 && copy[len - 1] == '\n') copy[len - 1] = '\0';
    if (copy[0] == '\0') { free(copy); free(lines); *n = 0; return (char **)calloc(1, sizeof(char *)); }
    char *cur = copy;
    while (1) {
        char *nl = strchr(cur, '\n');
        char *end = nl ? nl : cur + strlen(cur);
        size_t llen = (size_t)(end - cur);
        if (cnt == cap) { cap *= 2; lines = (char **)realloc(lines, cap * sizeof(char *)); if (!lines) { free(copy); return NULL; } }
        char *line = (char *)malloc(llen + 1);
        if (!line) { free(copy); free(lines); return NULL; }
        memcpy(line, cur, llen); line[llen] = 0;
        lines[cnt++] = line;
        if (!nl) break;
        cur = nl + 1;
    }
    free(copy);
    *n = cnt;
    return lines;
}

/* simple hash to speed equality */
static unsigned long hval(const char *s) {
    unsigned long h = 5381; int c;
    while ((c = (unsigned char)*s++)) h = ((h << 5) + h) + (unsigned)c;
    return h;
}

int wubudiff_text(const char *a, const char *b, wubudiff_hunk *out, int cap) {
    if (!a || !b) return 0;
    int na, nb;
    char **la = split_lines(a, &na);
    char **lb = split_lines(b, &nb);
    if (!la || !lb) {
        if (la) { for (int k=0;k<na;k++) free(la[k]); free(la); }
        if (lb) { for (int k=0;k<nb;k++) free(lb[k]); free(lb); }
        return 0;
    }

    /* Wagner-Fischer LCS length table */
    int *dp = (int *)malloc((size_t)(na + 1) * (nb + 1) * sizeof(int));
    if (!dp) {
        for (int k=0;k<na;k++) free(la[k]);
        free(la);
        for (int k=0;k<nb;k++) free(lb[k]);
        free(lb);
        return 0;
    }
    for (int i = 0; i <= na; i++) dp[(size_t)i * (nb + 1)] = 0;
    for (int j = 0; j <= nb; j++) dp[j] = 0;
    for (int i = na - 1; i >= 0; i--)
        for (int j = nb - 1; j >= 0; j--)
            dp[(size_t)i * (nb + 1) + j] =
                (hval(la[i]) == hval(lb[j]) && strcmp(la[i], lb[j]) == 0)
                ? dp[(size_t)(i + 1) * (nb + 1) + j + 1] + 1
                : (dp[(size_t)(i + 1) * (nb + 1) + j] > dp[(size_t)i * (nb + 1) + j + 1]
                   ? dp[(size_t)(i + 1) * (nb + 1) + j]
                   : dp[(size_t)i * (nb + 1) + j + 1]);

    int i = 0, j = 0, cnt = 0;
    while (i < na || j < nb) {
        if (i < na && j < nb && hval(la[i]) == hval(lb[j]) && strcmp(la[i], lb[j]) == 0) {
            if (cnt < cap) { out[cnt].op = WUBUDIFF_EQ; out[cnt].a_line = i; out[cnt].b_line = j; }
            cnt++; i++; j++;
        } else if (j < nb && (i >= na || dp[(size_t)i * (nb + 1) + j + 1] >= dp[(size_t)(i + 1) * (nb + 1) + j])) {
            if (cnt < cap) { out[cnt].op = WUBUDIFF_INS; out[cnt].a_line = -1; out[cnt].b_line = j; }
            cnt++; j++;
        } else {
            if (cnt < cap) { out[cnt].op = WUBUDIFF_DEL; out[cnt].a_line = i; out[cnt].b_line = -1; }
            cnt++; i++;
        }
    }
    free(dp);
    for (int k = 0; k < na; k++) free(la[k]);
    free(la);
    for (int k = 0; k < nb; k++) free(lb[k]);
    free(lb);
    return cnt;
}

int wubudiff_count(const char *a, const char *b) {
    return wubudiff_text(a, b, NULL, 0);
}
