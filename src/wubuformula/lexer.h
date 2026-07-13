#ifndef WUBUFORMULA_LEXER_H
#define WUBUFORMULA_LEXER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Token kinds for the formula grammar. Operator ids double as the AST op id. */
typedef enum {
    T_EOF = 0,
    T_NUM,        /* numeric literal */
    T_STR,        /* "quoted string" */
    T_IDENT,      /* function name / boolean / sheet name */
    T_REF,        /* cell ref, possibly sheet! qualified, possibly $ anchored */
    T_RANGE,      /* a:b produced when a ref is followed by ':' ref */
    T_LPAREN, T_RPAREN, T_COMMA,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_CARET, T_AMP,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_PERCENT, T_COLON,
    T_ERR         /* lexer error (e.g. unterminated string) */
} wubu_tok_kind;

typedef struct {
    wubu_tok_kind kind;
    double num;          /* T_NUM */
    char *text;          /* owned: T_STR / T_IDENT / T_REF text */
    int op;              /* operator id for operator tokens */
    size_t pos;          /* source offset of token start */
} wubu_token;

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    wubu_token tok;      /* current token (after next()) */
    int err;
} wubu_lexer;

void wubu_lexer_init(wubu_lexer *L, const char *src, size_t len);
/* Advance to the next token. Returns 0 on success, -1 on lexer error. */
int wubu_lexer_next(wubu_lexer *L);
void wubu_token_free(wubu_token *t);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_LEXER_H */
