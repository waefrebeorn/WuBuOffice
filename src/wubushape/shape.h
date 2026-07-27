/* shape.h -- minimal text shaping: Unicode Bidi visual reordering (INT-7).
 *
 * WuBuOffice apps render text LTR-only today; RTL scripts (Arabic, Hebrew)
 * come out backwards. This module is the missing shaping backend: given a
 * logical-order UTF-8 string and a base direction, it returns the visual-order
 * string (RTL runs reversed, neutrals resolved by context) so the renderer can
 * paint it correctly. A bounded, self-contained implementation of the common
 * Bidi cases (no full UBA, but correct for the dominant RTL/LTR/number mix).
 * C11, no third-party deps. */
#ifndef WUBUSHAPE_H
#define WUBUSHAPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SHAPE_LTR = 0, SHAPE_RTL = 1 } ShapeDir;

/* Reorder `text` (UTF-8, logical order) into visual order for base direction
 * `base`, writing at most `outcap` bytes into `out` (always NUL-terminated if
 * outcap>0). Returns the number of bytes written (excluding NUL). The caller
 * supplies `out`; size it >= strlen(text)+1. */
size_t shape_reorder(const char *text, ShapeDir base, char *out, size_t outcap);

/* True if the codepoint belongs to an RTL script block (Arabic/Hebrew/Syriac/
 * Thaana/etc.). Used by callers that want per-run direction without reorder. */
int shape_is_rtl_codepoint(unsigned long cp);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSHAPE_H */
