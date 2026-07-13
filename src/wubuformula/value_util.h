#ifndef WUBUFORMULA_VALUE_UTIL_H
#define WUBUFORMULA_VALUE_UTIL_H

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared coercion / comparison helpers used across the formula function
 * modules. Centralised here so every function file uses ONE implementation
 * (no per-file duplicates of strcasecmp_local etc.). */

/* Case-insensitive string compare (no locale). */
int wubu_strcasecmp(const char *a, const char *b);

/* Coerce a value to a number. *ok is set to 1 on success, 0 if the value
 * cannot be interpreted as a number (e.g. a non-numeric string). */
double wubu_to_num(const wubuval *v, int *ok);

/* Coerce a value to a boolean (truthy test). */
int wubu_to_bool(const wubuval *v);

/* Render a value to a const text view (NULL for numbers — caller renders).
 * The returned pointer is either v->str or a static string; do NOT free. */
const char *wubu_to_str(const wubuval *v);

/* Render a number to a freshly-allocated string (caller frees). */
char *wubu_num_to_str(double x);

/* Sum all numeric values in a flat array; sets *cnt to how many were numeric. */
double wubu_sum_flat(const wubuval *flat, int n, int *cnt);

/* Evaluate an Excel-style criteria expression against a value.
 *   number  -> equal to number
 *   ">n" "<n" ">=n" "<=n" "<>n" "=n"  -> numeric comparison
 *   "txt"  -> exact, case-insensitive string match
 * Returns 1 if `v` satisfies the criterion. */
int wubu_match_criteria(const wubuval *v, const wubuval *crit);

/* Collect the numeric values from a flat array into a freshly-allocated
 * (caller-frees) double[] of length *out_n. Returns NULL if none numeric. */
double *wubu_numeric_flat(const wubuval *flat, int n, int *out_n);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_VALUE_UTIL_H */
