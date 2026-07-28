/* test_codefold.c -- headless unit test for the extracted opaque CodeFold
 * engine. Verifies fold toggle (hide/show a brace region) and sym-panel toggle.
 * Uses the WuBuPad Doc + lexer to drive a real fold region.
 */
#include "codefold.h"
#include "doc.h"
#include "lex.h"

#include <stdio.h>
#include <string.h>

int main(void){
    int fails = 0;
    /* a small C-ish buffer with one brace block on lines 1..3 */
    const char *src =
        "int main() {\n"
        "    int x = 1;\n"
        "    return x;\n"
        "}\n"
        "int other() { return 0; }\n";
    Doc *d = doc_create(src);
    CodeFold *cf = codefold_create();
    if (!d || !cf){ fprintf(stderr, "alloc failed\n"); return 1; }

    if (codefold_symmode(cf)){ fprintf(stderr, "[init] sym off by default\n"); fails++; }
    codefold_sym_toggle(cf);
    if (!codefold_symmode(cf)){ fprintf(stderr, "[sym] toggle on\n"); fails++; }
    codefold_sym_toggle(cf);
    if (codefold_symmode(cf)){ fprintf(stderr, "[sym] toggle off\n"); fails++; }

    /* nothing folded yet */
    if (codefold_folded_count(cf) != 0){ fprintf(stderr, "[fold] none yet\n"); fails++; }
    if (codefold_hidden(cf, 2)){ fprintf(stderr, "[fold] line2 not hidden\n"); fails++; }

    /* fold the block containing line 2 (inside main's braces) */
    codefold_toggle_block(cf, d, 2);
    int hidden = codefold_folded_count(cf);
    if (hidden <= 0){ fprintf(stderr, "[fold] expected lines hidden after toggle\n"); fails++; }
    if (!codefold_hidden(cf, 2)){ fprintf(stderr, "[fold] line2 should be hidden\n"); fails++; }

    /* toggle again -> unfold (back to 0) */
    codefold_toggle_block(cf, d, 2);
    if (codefold_folded_count(cf) != 0){ fprintf(stderr, "[fold] toggle again -> 0 hidden (got %d)\n", codefold_folded_count(cf)); fails++; }
    if (codefold_hidden(cf, 2)){ fprintf(stderr, "[fold] line2 should be visible again\n"); fails++; }

    /* fold region outside any braces (line of 'other') toggles nothing */
    codefold_toggle_block(cf, d, 4);
    if (codefold_folded_count(cf) != 0){ fprintf(stderr, "[fold] line4 has no enclosing block\n"); fails++; }

    codefold_destroy(cf);
    doc_free(d);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: codefold (toggle hide/show brace region, sym toggle, no-region)\n");
    return 0;
}
