/* WuBuOffice -- wubuformula/ast
 * AST node constructors/destructor. */

#include "ast.h"

#include <stdlib.h>
#include <string.h>

static ast *mk(ast_kind k) {
    ast *n = calloc(1, sizeof *n);
    n->kind = k;
    return n;
}

ast *ast_num(double x) { ast *n = mk(N_NUM); n->num = x; return n; }
ast *ast_str(const char *s) { ast *n = mk(N_STR); n->str = s ? strdup(s) : strdup(""); return n; }
ast *ast_bool(int b) { ast *n = mk(N_BOOL); n->boolean = b ? 1 : 0; return n; }
ast *ast_ref(const wubucell_ref *r) { ast *n = mk(N_REF); n->ref = *r; return n; }
ast *ast_range(const wubucell_ref *a, const wubucell_ref *b) { ast *n = mk(N_RANGE); n->range.a = *a; n->range.b = *b; return n; }
ast *ast_unary(int op, ast *c) { ast *n = mk(N_UNARY); n->op = op; n->child = c; return n; }
ast *ast_binary(int op, ast *l, ast *r) { ast *n = mk(N_BINARY); n->op = op; n->l = l; n->r = r; return n; }
ast *ast_func(const char *name, ast **args, int nargs) {
    ast *n = mk(N_FUNC);
    /* Take ownership of `name`: the parser passes a heap-allocated string it
     * no longer needs, so strdup'ing here would orphan the caller's copy. */
    n->str = (char *)name;
    n->nargs = nargs;
    /* Take ownership of the caller's args array (the parser always passes a
     * heap-allocated vector). Copying here would orphan the caller's array
     * and double the allocation cost. */
    n->args = args;
    return n;
}

void ast_free(ast *n) {
    if (!n) return;
    if (n->kind == N_STR || n->kind == N_FUNC) free(n->str);
    if (n->kind == N_FUNC) {
        for (int i = 0; i < n->nargs; i++) ast_free(n->args[i]);
        free(n->args);
    }
    ast_free(n->child);
    ast_free(n->l);
    ast_free(n->r);
    free(n);
}
