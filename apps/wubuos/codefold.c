/* codefold.c -- opaque code-folding + function-list state (see codefold.h).
 * Ported out of view_editor.c so the editor no longer owns the fold array.
 */
#include "codefold.h"

#include "doc.h"   /* WuBuPad piece-table Doc */
#include "lex.h"   /* WuBuPad lexer: lex_folds */

#include <stdlib.h>
#include <string.h>

#define CF_MAXLINES 4096

struct CodeFold {
    char  hidden[CF_MAXLINES];  /* per-line hide flag */
    int   sym_mode;             /* 0 off, 1 on (function-list panel) */
};

CodeFold *codefold_create(void){ return calloc(1, sizeof(CodeFold)); }
void codefold_destroy(CodeFold *cf){ free(cf); }

int codefold_hidden(const CodeFold *cf, int line){
    if (!cf || line < 0 || line >= CF_MAXLINES) return 0;
    return cf->hidden[line];
}

int codefold_folded_count(const CodeFold *cf){
    if (!cf) return 0;
    int c = 0;
    for (int i=0;i<CF_MAXLINES;i++) if (cf->hidden[i]) c++;
    return c;
}

int codefold_symmode(const CodeFold *cf){ return cf ? cf->sym_mode : 0; }
void codefold_sym_toggle(CodeFold *cf){ if (cf) cf->sym_mode ^= 1; }

/* Toggle the fold state of the brace block containing the caret line. */
void codefold_toggle_block(CodeFold *cf, const void *doc, int caret_line){
    if (!cf || !doc) return;
    char *t = doc_text((const Doc*)doc);
    size_t n = doc_length((const Doc*)doc);
    free(t);
    LexFold fs[256];
    size_t nf = lex_folds(t = doc_text((const Doc*)doc), n, fs, 256);
    free(t);
    int region = -1;
    for (size_t i=0;i<nf;i++) if ((int)fs[i].start <= caret_line && (int)fs[i].end > caret_line){ region = (int)i; break; }
    if (region < 0) return;
    size_t a = fs[region].start, b = fs[region].end;
    int is_folded = (a+1 < CF_MAXLINES) ? cf->hidden[a+1] : 0;
    for (size_t ln = a+1; ln < b && ln < CF_MAXLINES; ln++) cf->hidden[ln] = is_folded ? 0 : 1;
}
