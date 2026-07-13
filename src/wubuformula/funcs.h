#ifndef WUBUFORMULA_FUNCS_H
#define WUBUFORMULA_FUNCS_H

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A function implementation receives:
 *   args    - the evaluated, per-argument values (length nargs)
 *   flat    - every element of any range argument, flattened in order
 *             (numbers/text/bool/empty; used by aggregate functions)
 *   flatn   - number of flattened elements
 *   out     - the result (callee fills; owns any str)
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */
typedef void (*wubu_func_impl)(const wubuval *args, int nargs,
                               const wubuval *flat, int flatn, wubuval *out);

/* Look up a function by name (case-insensitive). Returns the impl, or NULL if
 * unknown (caller should produce #NAME?). */
wubu_func_impl wubu_func_lookup(const char *name);

/* Number of registered functions (for diagnostics/tests). */
int wubu_func_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_FUNCS_H */
