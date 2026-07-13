#ifndef WUBUFORMULA_EVAL_H
#define WUBUFORMULA_EVAL_H

#include "value.h"
#include "ast.h"
#include "funcs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Evaluate an already-parsed AST. `resolve` is invoked for REF/RANGE leaves;
 * `ctx` is passed through. `cycle` (if non-NULL) detects circular references
 * via a generation id: the resolver sets *cycle=1 when a cell is re-entered.
 *
 * Returns 0 on success (out filled), -1 if evaluation could not proceed
 * (out already set to an error value in that case). */
int wubu_eval(const ast *node, wubuformula_resolver resolve, void *ctx,
              wubuval *out);

/* High-level: parse + evaluate a formula string. `src` must NOT include the
 * leading '='. On parse error returns -1 and out is set to #PARSE!. */
int wubu_formula_eval(const char *src, wubuformula_resolver resolve, void *ctx,
                      wubuval *out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_EVAL_H */
