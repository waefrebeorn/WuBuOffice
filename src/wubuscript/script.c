/* script.c -- dependency-free C11 safe scripting host (see script.h). */
#include "script.h"
#include "eval.h"
#include "funcs.h"
#include "value.h"
#include "value_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Sandboxed registry: only math / aggregate / IO-safe helpers are registered
 * by wubu_formula_register_all(); no FS/process/system functions exist, so a
 * document-embedded script cannot escape the host. */

static int g_registered = 0;
static void ensure_registry(void){
    if (g_registered) return;
    wubu_formula_register_all();
    g_registered = 1;
}

/* Resolver that rejects any external cell reference (our scripts carry no
 * cell refs after variable substitution, so this just enforces sandboxing). */
static int no_refs(void *ctx, const wubucell_ref *ref, wubuval *out){
    (void)ctx; (void)ref; (void)out;
    return -1;
}

/* Substitute named variables with their numeric values. Identifiers
 * [A-Za-z_][A-Za-z0-9_]* are resolved via `resolve`; unknown identifiers are
 * left intact (the formula evaluator then yields #NAME? -> we fail). */
static char *substitute(const char *expr, wubuscript_resolve resolve, void *ctx){
    size_t n = strlen(expr);
    char *out = malloc(n * 8 + 16);   /* room for expanded numbers */
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < n; ){
        char c = expr[i];
        if (isalpha((unsigned char)c) || c == '_'){
            size_t j = i;
            while (j < n && (isalnum((unsigned char)expr[j]) || expr[j]=='_')) j++;
            size_t len = j - i;
            char name[64];
            if (len >= sizeof name) len = sizeof(name)-1;
            memcpy(name, expr+i, len); name[len]=0;
            double v;
            if (resolve && resolve(name, &v, ctx) == 0){
                oi += (size_t)snprintf(out+oi, (n*8+16)-oi, "%.10g", v);
            } else {
                memcpy(out+oi, name, len); oi += len;
            }
            i = j;
        } else {
            out[oi++] = c; i++;
        }
    }
    out[oi] = 0;
    return out;
}

int script_eval(const char *expr, wubuscript_resolve resolve, void *ctx,
                double *out){
    if (!expr || !out) return -1;
    *out = 0;
    ensure_registry();
    char *sub = substitute(expr, resolve, ctx);
    if (!sub) return -1;
    wubuval v;
    memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(sub, no_refs, NULL, &v);
    free(sub);
    if (rc != 0) return -1;
    if (v.kind == WV_NUM){
        *out = v.num;
        return 0;
    }
    return -1;   /* non-numeric / error result */
}

char *script_eval_str(const char *expr, wubuscript_resolve resolve, void *ctx){
    if (!expr) return NULL;
    ensure_registry();
    char *sub = substitute(expr, resolve, ctx);
    if (!sub) return NULL;
    wubuval v;
    memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(sub, no_refs, NULL, &v);
    free(sub);
    if (rc != 0) return NULL;
    char *s = wubu_num_to_str(v.num);
    return s;
}
