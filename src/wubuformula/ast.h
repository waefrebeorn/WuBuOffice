#ifndef WUBUFORMULA_AST_H
#define WUBUFORMULA_AST_H

#include "value.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AST node kinds. */
typedef enum {
    N_NUM,          /* literal number */
    N_STR,          /* literal string */
    N_BOOL,         /* TRUE / FALSE */
    N_REF,          /* single cell reference */
    N_RANGE,        /* cell range reference */
    N_UNARY,        /* unary +/-/% */
    N_BINARY,       /* binary operator */
    N_FUNC,         /* function call: name + args */
    N_PAREN         /* parenthesized (folded away by the parser) */
} ast_kind;

typedef struct ast ast;

struct ast {
    ast_kind kind;
    /* literal payload */
    double num;
    char *str;          /* owned for N_STR; function name for N_FUNC */
    int boolean;
    wubucell_ref ref;   /* N_REF / range endpoints */
    wubucell_range range;

    int op;             /* operator token id (N_UNARY/N_BINARY) */
    ast *child;         /* N_UNARY / N_PAREN */
    ast **args;         /* N_FUNC argument list (owned) */
    int nargs;
    ast *l, *r;         /* N_BINARY */
};

ast *ast_num(double x);
ast *ast_str(const char *s);
ast *ast_bool(int b);
ast *ast_ref(const wubucell_ref *r);
ast *ast_range(const wubucell_ref *a, const wubucell_ref *b);
ast *ast_unary(int op, ast *c);
ast *ast_binary(int op, ast *l, ast *r);
ast *ast_func(const char *name, ast **args, int nargs);
void ast_free(ast *n);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_AST_H */
