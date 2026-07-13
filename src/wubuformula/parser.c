/* WuBuOffice -- wubuformula/parser
 *
 * Clean-room, from-scratch (SLERM). Grammar (lowest precedence last):
 *   expr    := compare
 *   compare := concat (('='|'<>'|'<'|'>'|'<='|'>=') concat)*
 *   concat  := addop ('&' addop)*
 *   addop   := term (('+'|'-') term)*
 *   term    := power (('*'|'/') power)*
 *   power   := unary ('^' unary)?
 *   unary   := ('+'|'-') unary | postfix
 *   postfix := primary ('%' primary)?        (% is postfix percent = /100)
 *   primary := NUM | STR | BOOL | REF | REF ':' REF | func | '(' expr ')'
 *   func    := IDENT '(' (expr (',' expr)*)? ')'
 */

#include "parser.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static int strcasecmp_local(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

typedef struct {
    wubu_lexer L;
    char *errbuf; size_t errcap;
    int failed;
} parser;

static void perr(parser *p, const char *m) {
    p->failed = 1;
    if (p->errbuf && p->errcap) {
        snprintf(p->errbuf, p->errcap, "%s", m);
        p->errbuf[p->errcap - 1] = '\0';
    }
}

/* parsed cell ref text (e.g. "Sheet1!$A$1") into wubucell_ref */
static int parse_ref_text(const char *t, wubucell_ref *r) {
    memset(r, 0, sizeof *r);
    r->sheet = -1; /* current */
    const char *s = t;
    /* sheet qualifier (either 'Name'! or Name!) */
    const char *bang = strchr(s, '!');
    if (bang) {
        size_t nl = (size_t)(bang - s);
        /* strip surrounding single quotes if present */
        const char *ns = s; size_t ne = nl;
        if (nl >= 2 && s[0] == '\'') { ns = s + 1; ne = nl - 2; }
        if (ne >= sizeof r->sheet_name) ne = sizeof r->sheet_name - 1;
        memcpy(r->sheet_name, ns, ne);
        r->sheet_name[ne] = '\0';
        r->sheet = -2; /* qualified by name */
        s = bang + 1;
    }
    if (*s == '$') { r->abs_col = 1; s++; }
    int col = 0;
    while (isalpha((unsigned char)*s)) { col = col * 26 + (toupper((unsigned char)*s) - 'A' + 1); s++; }
    r->col = col;
    if (*s == '$') { r->abs_row = 1; s++; }
    int row = 0;
    while (isdigit((unsigned char)*s)) { row = row * 10 + (*s - '0'); s++; }
    r->row = row;
    return 0;
}

static ast *parse_expr(parser *p);

static ast *parse_primary(parser *p) {
    wubu_token *t = &p->L.tok;
    if (t->kind == T_NUM) {
        ast *n = ast_num(t->num);
        wubu_lexer_next(&p->L);
        return n;
    }
    if (t->kind == T_STR) {
        ast *n = ast_str(t->text);
        wubu_lexer_next(&p->L);
        return n;
    }
    if (t->kind == T_IDENT) {
        /* boolean literal or function call */
        if (strcasecmp_local(t->text, "TRUE") == 0) { wubu_lexer_next(&p->L); return ast_bool(1); }
        if (strcasecmp_local(t->text, "FALSE") == 0) { wubu_lexer_next(&p->L); return ast_bool(0); }
        /* function call */
        char *fname = strdup(t->text);
        wubu_lexer_next(&p->L);
        if (p->L.tok.kind != T_LPAREN) { perr(p, "expected '(' after function name"); free(fname); return NULL; }
        wubu_lexer_next(&p->L);
        ast **args = NULL; int nargs = 0, cap = 0;
        if (p->L.tok.kind != T_RPAREN) {
            for (;;) {
                ast *a = parse_expr(p);
                if (!a) { free(fname); for (int i = 0; i < nargs; i++) ast_free(args[i]); free(args); return NULL; }
                if (nargs == cap) { cap = cap ? cap*2 : 4; args = realloc(args, (size_t)cap * sizeof(ast*)); }
                args[nargs++] = a;
                if (p->L.tok.kind == T_COMMA) { wubu_lexer_next(&p->L); continue; }
                break;
            }
        }
        if (p->L.tok.kind != T_RPAREN) { perr(p, "expected ')'"); free(fname); for (int i = 0; i < nargs; i++) ast_free(args[i]); free(args); return NULL; }
        wubu_lexer_next(&p->L);
        return ast_func(fname, args, nargs);
    }
    if (t->kind == T_REF) {
        wubucell_ref a; parse_ref_text(t->text, &a);
        ast *first = ast_ref(&a);
        wubu_lexer_next(&p->L);
        if (p->L.tok.kind == T_COLON) {
            wubu_lexer_next(&p->L);
            if (p->L.tok.kind != T_REF) { perr(p, "expected cell ref after ':'"); ast_free(first); return NULL; }
            wubucell_ref b; parse_ref_text(p->L.tok.text, &b);
            ast *rng = ast_range(&a, &b);
            wubu_lexer_next(&p->L);
            return rng;
        }
        return first;
    }
    if (t->kind == T_LPAREN) {
        wubu_lexer_next(&p->L);
        ast *e = parse_expr(p);
        if (!e) return NULL;
        if (p->L.tok.kind != T_RPAREN) { perr(p, "expected ')'"); ast_free(e); return NULL; }
        wubu_lexer_next(&p->L);
        return e;
    }
    perr(p, "unexpected token");
    return NULL;
}

static ast *parse_postfix(parser *p) {
    ast *n = parse_primary(p);
    if (!n) return NULL;
    while (p->L.tok.kind == T_PERCENT) {
        wubu_lexer_next(&p->L);
        /* percent = divide by 100: represent as binary '/' 0.01 */
        n = ast_binary('/', n, ast_num(100.0));
    }
    return n;
}

static ast *parse_unary(parser *p) {
    wubu_token *t = &p->L.tok;
    if (t->kind == T_PLUS) { wubu_lexer_next(&p->L); return parse_unary(p); }
    if (t->kind == T_MINUS) { wubu_lexer_next(&p->L); return ast_unary('-', parse_unary(p)); }
    return parse_postfix(p);
}

static ast *parse_power(parser *p) {
    ast *l = parse_unary(p);
    if (!l) return NULL;
    if (p->L.tok.kind == T_CARET) {
        wubu_lexer_next(&p->L);
        ast *r = parse_power(p); /* right-associative */
        if (!r) { ast_free(l); return NULL; }
        return ast_binary('^', l, r);
    }
    return l;
}

static ast *parse_term(parser *p) {
    ast *l = parse_power(p);
    if (!l) return NULL;
    for (;;) {
        wubu_tok_kind k = p->L.tok.kind;
        if (k == T_STAR || k == T_SLASH) {
            int op = p->L.tok.op;
            wubu_lexer_next(&p->L);
            ast *r = parse_power(p);
            if (!r) { ast_free(l); return NULL; }
            l = ast_binary(op, l, r);
        } else break;
    }
    return l;
}

static ast *parse_addop(parser *p) {
    ast *l = parse_term(p);
    if (!l) return NULL;
    for (;;) {
        wubu_tok_kind k = p->L.tok.kind;
        if (k == T_PLUS || k == T_MINUS) {
            int op = p->L.tok.op;
            wubu_lexer_next(&p->L);
            ast *r = parse_term(p);
            if (!r) { ast_free(l); return NULL; }
            l = ast_binary(op, l, r);
        } else break;
    }
    return l;
}

static ast *parse_concat(parser *p) {
    ast *l = parse_addop(p);
    if (!l) return NULL;
    while (p->L.tok.kind == T_AMP) {
        wubu_lexer_next(&p->L);
        ast *r = parse_addop(p);
        if (!r) { ast_free(l); return NULL; }
        l = ast_binary('&', l, r);
    }
    return l;
}

static ast *parse_compare(parser *p) {
    ast *l = parse_concat(p);
    if (!l) return NULL;
    for (;;) {
        wubu_tok_kind k = p->L.tok.kind;
        if (k == T_EQ || k == T_NE || k == T_LT || k == T_GT || k == T_LE || k == T_GE) {
            int op = p->L.tok.op;
            wubu_lexer_next(&p->L);
            ast *r = parse_concat(p);
            if (!r) { ast_free(l); return NULL; }
            l = ast_binary(op, l, r);
        } else break;
    }
    return l;
}

static ast *parse_expr(parser *p) { return parse_compare(p); }

ast *wubu_parse(const char *src, size_t len, char *errbuf, size_t errcap) {
    parser p; memset(&p, 0, sizeof p);
    p.errbuf = errbuf; p.errcap = errcap;
    wubu_lexer_init(&p.L, src, len);
    wubu_lexer_next(&p.L);
    if (p.L.err) { perr(&p, "lexer error"); return NULL; }
    ast *root = parse_expr(&p);
    if (!root) return NULL;
    if (p.L.tok.kind != T_EOF) { perr(&p, "trailing tokens"); ast_free(root); return NULL; }
    return root;
}
