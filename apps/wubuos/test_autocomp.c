/* test_autocomp.c -- headless unit test for the extracted opaque AutoComp
 * engine (no GUI/SDL). Verifies builtin + document-word collection, prefix
 * filtering, selection move, and completion application.
 */
#include "autocomp.h"
#include "doc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    int fails = 0;
    Doc *d = doc_create(
        "int main(void) { int total = 0; for (int i=0;i<10;i++) total += i; }\n"
        "integerize total\n");
    AutoComp *ac = autocomp_create();
    if (!d || !ac){ fprintf(stderr, "alloc failed\n"); return 1; }

    /* open on the word under the caret. Put the caret at 0 so the prefix is
     * empty -> all builtins + all identifiers are collected. */
    doc_set_cursor(d, 0);
    autocomp_open(ac, d);
    if (!autocomp_opened(ac)){ fprintf(stderr, "[open] popup should open\n"); fails++; }
    int n = autocomp_count(ac);
    if (n <= 0){ fprintf(stderr, "[open] no candidates collected\n"); fails++; }

    /* builtin 'int' must be present; document id 'total' must be present. */
    int has_int = 0, has_total = 0;
    for (int i=0;i<n;i++){
        const char *c = autocomp_candidate(ac, i);
        if (!strcmp(c, "int")) has_int = 1;
        if (!strcmp(c, "total")) has_total = 1;
    }
    if (!has_int){ fprintf(stderr, "[collect] builtin 'int' missing\n"); fails++; }
    if (!has_total){ fprintf(stderr, "[collect] doc id 'total' missing\n"); fails++; }

    /* selection move clamps */
    autocomp_move(ac, -1);                 /* already at 0 -> stays 0 */
    if (autocomp_selected(ac) != 0){ fprintf(stderr, "[move] up past 0\n"); fails++; }
    autocomp_move(ac, +1);
    if (autocomp_selected(ac) != 1){ fprintf(stderr, "[move] down -> 1\n"); fails++; }

    /* accept the selected candidate: inserts the word at the caret (caret=0,
     * empty prefix -> just inserts the candidate word). */
    int sel = autocomp_selected(ac);
    const char *word = autocomp_candidate(ac, sel);
    size_t before = doc_length(d);
    if (!autocomp_accept(ac, d)){ fprintf(stderr, "[accept] failed\n"); fails++; }
    else {
        if (autocomp_opened(ac)){ fprintf(stderr, "[accept] popup still open\n"); fails++; }
        size_t after = doc_length(d);
        if (after <= before){ fprintf(stderr, "[accept] doc did not grow\n"); fails++; }
        /* the inserted word should now lead the buffer */
        char *t = doc_text(d);
        if (strncmp(t, word, strlen(word)) != 0){
            fprintf(stderr, "[accept] '%s' not inserted at caret (got '%.20s')\n", word, t);
            fails++;
        }
        free(t);
    }

    /* close + reopen then close clears state */
    autocomp_open(ac, d);
    autocomp_close(ac);
    if (autocomp_opened(ac)){ fprintf(stderr, "[close] still open\n"); fails++; }

    autocomp_destroy(ac);
    doc_free(d);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: autocomp (collect builtins+docs, prefix, move, accept, close)\n");
    return 0;
}
