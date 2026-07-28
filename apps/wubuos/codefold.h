/* codefold.h -- opaque code-folding + function-list state for the editor.
 *
 * Owns the per-line hidden flags (brace-region folding) and the function-list
 * panel toggle. The render loop asks codefold_hidden() to skip folded lines
 * and codefold_symmode() to draw the panel; it never touches the raw array.
 *
 * C11, opaque struct, no god-header: include only this file.
 */
#ifndef WUBUOS_CODEFOLD_H
#define WUBUOS_CODEFOLD_H

typedef struct CodeFold CodeFold;

/* Create an empty fold state (max 4096 lines tracked). */
CodeFold *codefold_create(void);
void      codefold_destroy(CodeFold *cf);

/* True if `line` (0-based) is currently hidden by a fold. */
int codefold_hidden(const CodeFold *cf, int line);

/* Count of currently-hidden (folded) lines. */
int codefold_folded_count(const CodeFold *cf);

/* Toggle the fold state of the brace block containing the caret line of
 * `doc` (WuBuPad Doc), using the lexer's fold regions. */
void codefold_toggle_block(CodeFold *cf, const void *doc, int caret_line);

/* Function-list panel on/off. */
int  codefold_symmode(const CodeFold *cf);
void codefold_sym_toggle(CodeFold *cf);

#endif /* WUBUOS_CODEFOLD_H */
