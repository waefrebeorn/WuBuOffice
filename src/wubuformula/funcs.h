#ifndef WUBUFORMULA_FUNCS_H
#define WUBUFORMULA_FUNCS_H

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-range-argument geometry: when argument i was a RANGE reference, the
 * evaluator resolves it into a row-major grid `grid` (rows*cols values,
 * owned by the evaluator; callee must NOT free) with the given dimensions.
 * `cols[i] < 0` means argument i was NOT a range (use `args[i]`/`flat`). */
typedef struct {
    const wubuval *grid; /* row-major, length rows*cols (or NULL if not a range) */
    int rows, cols;
} wubu_func_range;

/* A function implementation receives:
 *   args    - the evaluated, per-argument values (length nargs)
 *   flat    - every element of any range argument, flattened in order
 *             (numbers/text/bool/empty; used by aggregate functions)
 *   flatn   - number of flattened elements
 *   ranges  - per-argument range geometry (ranges[i].cols > 0 iff arg i was a
 *             RANGE); grids are evaluator-owned and valid only during the call
 *   out     - the result (callee fills; owns any str)
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */
typedef void (*wubu_func_impl)(const wubuval *args, int nargs,
                               const wubuval *flat, int flatn,
                               const wubu_func_range *ranges, wubuval *out);

/* Look up a function by name (case-insensitive). Returns the impl, or NULL if
 * unknown (caller should produce #NAME?). */
wubu_func_impl wubu_func_lookup(const char *name);

/* Number of registered functions (for diagnostics/tests). */
int wubu_func_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_FUNCS_H */
