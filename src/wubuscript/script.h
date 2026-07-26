/* script.h -- dependency-free C11 safe scripting host (wubuscript).
 *
 * A bounded, sandboxable expression/script host built on wubuformula. It
 * evaluates a formula/expression string and returns its value, with a
 * caller-supplied variable resolver. The function registry is deliberately
 * restricted to a math/IO-safe subset (no file/process/system calls) so
 * document-embedded scripts cannot escape the host.
 *
 * NOTE ON SCOPE: this is a formula-based script host, NOT a full Lua VM.
 * A from-scratch Lua 5.x interpreter is ~10k lines and pulls in a large
 * surface; the project's no-third-party-C posture excludes embedding one.
 * For scripting needs we reuse the already-audited wubuformula evaluator,
 * which covers arithmetic, variables, aggregates (SUM/AVG/MIN/MAX) and a
 * curated function set. If a true Lua runtime is later required, it should
 * be added as an optional, allowlist-gated extension -- not into core. */
#ifndef WUBUOFFICE_SCRIPT_H
#define WUBUOFFICE_SCRIPT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Variable resolver: called by the evaluator for identifiers that are not
 * functions. Return 0 and set *out to resolve; return non-zero to signal
 * "unknown variable" (the script fails). `ctx` is the user pointer passed
 * to script_eval. */
typedef int (*wubuscript_resolve)(const char *name, double *out, void *ctx);

/* Evaluate `expr`. On success returns 0 and sets *out. On error (parse,
 * unknown var, div-by-zero, blacklisted call) returns -1 and *out is 0. */
int script_eval(const char *expr, wubuscript_resolve resolve, void *ctx,
                 double *out);

/* Like script_eval but returns a freshly-allocated string result (e.g. for
 * string-valued formulas); caller frees. Returns NULL on error. */
char *script_eval_str(const char *expr, wubuscript_resolve resolve, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_SCRIPT_H */
