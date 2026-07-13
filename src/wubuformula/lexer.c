/* WuBuOffice -- wubuformula/lexer
 * Formula tokenizer. Handles numbers, quoted strings, identifiers, cell
 * references (with $ anchors and Sheet! qualification), ranges, and operators.
 *
 * Clean-room, from-scratch (SLERM). */

#include "lexer.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void wubu_token_free(wubu_token *t) {
    if (t->text) { free(t->text); t->text = NULL; }
    t->kind = T_EOF;
}

void wubu_lexer_init(wubu_lexer *L, const char *src, size_t len) {
    L->src = src; L->len = len; L->pos = 0; L->err = 0;
    memset(&L->tok, 0, sizeof L->tok);
}

static int is_letter(int c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_alnum(int c) { return isalnum((unsigned char)c) || c == '_' || c == '.'; }

/* Parse a cell reference starting at L->pos, possibly with a leading
 * Sheet! qualifier. Fills *out (text kept in the token). Returns 1 if a ref
 * was consumed, 0 otherwise. On success L->pos is advanced past the ref. */
static int scan_ref(wubu_lexer *L, wubu_token *t) {
    size_t start = L->pos;
    size_t i = L->pos;
    /* optional Sheet! qualifier (letters/digits/quoted) */
    /* quoted sheet name: 'My Sheet'! */
    if (L->src[i] == '\'') {
        size_t j = i + 1;
        while (j < L->len && L->src[j] != '\'') j++;
        if (j >= L->len) return 0;
        i = j + 1;
        if (i < L->len && L->src[i] == '!') i++;
        else return 0;
    } else {
        size_t j = i;
        while (j < L->len && (is_letter(L->src[j]) || isdigit((unsigned char)L->src[j]) || L->src[j] == '_')) j++;
        if (j > i && i < L->len && L->src[j] == '!') i = j + 1;
        else i = L->pos; /* not a sheet qualifier */
    }
    /* optional $ + column letters */
    int col = 0, have_col = 0;
    if (i < L->len && L->src[i] == '$') i++;
    size_t cs = i;
    while (i < L->len && isalpha((unsigned char)L->src[i])) i++;
    if (i > cs) { have_col = 1; for (size_t k = cs; k < i; k++) col = col * 26 + (toupper((unsigned char)L->src[k]) - 'A' + 1); }
    /* optional $ + row digits */
    int row = 0, have_row = 0;
    if (i < L->len && L->src[i] == '$') i++;
    size_t rs = i;
    while (i < L->len && isdigit((unsigned char)L->src[i])) i++;
    if (i > rs) { have_row = 1; for (size_t k = rs; k < i; k++) row = row * 10 + (L->src[k] - '0'); }
    if (!have_col && !have_row) return 0;
    /* A1-style refs require a row; column-only tokens ("A") are identifiers. */
    if (!have_row) return 0;
    /* require at least column or row present (we have both above) */
    t->kind = T_REF;
    size_t tl = i - start;
    t->text = malloc(tl + 1);
    memcpy(t->text, L->src + start, tl);
    t->text[tl] = '\0';
    L->pos = i;
    (void)col; (void)row;
    return 1;
}

static void set_op(wubu_token *t, wubu_tok_kind kind, int op) { t->kind = kind; t->op = op; }

int wubu_lexer_next(wubu_lexer *L) {
    wubu_token_free(&L->tok);
    memset(&L->tok, 0, sizeof L->tok);
    while (L->pos < L->len && isspace((unsigned char)L->src[L->pos])) L->pos++;
    if (L->pos >= L->len) { L->tok.kind = T_EOF; return 0; }
    size_t i = L->pos;
    char c = L->src[i];

    /* number */
    if (isdigit((unsigned char)c) || (c == '.' && i + 1 < L->len && isdigit((unsigned char)L->src[i+1]))) {
        size_t j = i;
        while (j < L->len && (isdigit((unsigned char)L->src[j]) || L->src[j] == '.' || L->src[j] == 'e' || L->src[j] == 'E'
               || ((L->src[j] == '+' || L->src[j] == '-') && j > 0 && (L->src[j-1] == 'e' || L->src[j-1] == 'E')))) j++;
        char buf[64];
        size_t n = j - i; if (n >= sizeof buf) n = sizeof buf - 1;
        memcpy(buf, L->src + i, n); buf[n] = '\0';
        L->tok.kind = T_NUM;
        L->tok.num = strtod(buf, NULL);
        L->pos = j; L->tok.pos = i;
        return 0;
    }
    /* string literal */
    if (c == '"') {
        size_t j = i + 1; size_t cap = 16, n = 0; char *s = malloc(cap);
        while (j < L->len) {
            if (L->src[j] == '"') {
                if (j + 1 < L->len && L->src[j+1] == '"') { s[n++] = '"'; j += 2; continue; } /* escaped "" */
                break;
            }
            if (n + 1 >= cap) { cap *= 2; s = realloc(s, cap); }
            s[n++] = L->src[j++];
        }
        if (j >= L->len) { free(s); L->err = 1; L->tok.kind = T_ERR; return -1; }
        s[n] = '\0';
        L->tok.kind = T_STR; L->tok.text = s; L->pos = j + 1; L->tok.pos = i;
        return 0;
    }
    /* ref or identifier */
    if (is_letter(c) || c == '$' || c == '\'') {
        /* try a cell reference first */
        wubu_lexer save = *L;
        if (scan_ref(L, &L->tok)) { L->tok.pos = i; return 0; }
        *L = save; /* rewind; treat as identifier */
        size_t j = i;
        while (j < L->len && is_alnum(L->src[j])) j++;
        size_t n = j - i;
        char *s = malloc(n + 1); memcpy(s, L->src + i, n); s[n] = '\0';
        L->tok.kind = T_IDENT; L->tok.text = s; L->pos = j; L->tok.pos = i;
        return 0;
    }
    /* operators */
    switch (c) {
        case '(': set_op(&L->tok, T_LPAREN, '('); L->pos++; return 0;
        case ')': set_op(&L->tok, T_RPAREN, ')'); L->pos++; return 0;
        case ',': set_op(&L->tok, T_COMMA, ','); L->pos++; return 0;
        case '+': set_op(&L->tok, T_PLUS, '+'); L->pos++; return 0;
        case '-': set_op(&L->tok, T_MINUS, '-'); L->pos++; return 0;
        case '*': set_op(&L->tok, T_STAR, '*'); L->pos++; return 0;
        case '/': set_op(&L->tok, T_SLASH, '/'); L->pos++; return 0;
        case '^': set_op(&L->tok, T_CARET, '^'); L->pos++; return 0;
        case '&': set_op(&L->tok, T_AMP, '&'); L->pos++; return 0;
        case '%': set_op(&L->tok, T_PERCENT, '%'); L->pos++; return 0;
        case ':': set_op(&L->tok, T_COLON, ':'); L->pos++; return 0;
        case '=': set_op(&L->tok, T_EQ, '='); L->pos++; return 0;
        case '<':
            if (i + 1 < L->len && L->src[i+1] == '=') { set_op(&L->tok, T_LE, 256+1); L->pos += 2; return 0; }
            if (i + 1 < L->len && L->src[i+1] == '>') { set_op(&L->tok, T_NE, 256+2); L->pos += 2; return 0; }
            set_op(&L->tok, T_LT, '<'); L->pos++; return 0;
        case '>':
            if (i + 1 < L->len && L->src[i+1] == '=') { set_op(&L->tok, T_GE, 256+3); L->pos += 2; return 0; }
            set_op(&L->tok, T_GT, '>'); L->pos++; return 0;
        default:
            L->err = 1; L->tok.kind = T_ERR; L->pos++; return -1;
    }
}
