/* wubudiff.h — document comparison: LCS line diff between two texts,
 * producing a unified-style edit script. Powers the "Compare documents"
 * feature. */
#ifndef WUBUDIFF_H
#define WUBUDIFF_H
#include <stddef.h>

typedef enum { WUBUDIFF_EQ, WUBUDIFF_DEL, WUBUDIFF_INS } wubudiff_op;

typedef struct {
    wubudiff_op op;
    int a_line;   /* line index in text A (-1 for INS) */
    int b_line;   /* line index in text B (-1 for DEL) */
} wubudiff_hunk;

/* Compute the LCS line diff of `a` and `b` (each NUL-terminated). Writes up
 * to `cap` hunks into `out`, returns the count (may exceed cap). Uses a
 * hash-optimized DP. */
int wubudiff_text(const char *a, const char *b, wubudiff_hunk *out, int cap);

/* Convenience: how many hunks differ (nondestructive count). */
int wubudiff_count(const char *a, const char *b);

#endif
