/* test_bkmk.c -- headless unit test for the extracted opaque BkMk engine.
 * Pure logic (no Doc/GUI): toggle, sorted-uniqueness, has, jump next/prev.
 */
#include "bkmk.h"
#include <stdio.h>
#include <assert.h>

int main(void){
    int fails = 0;
    BkMk *b = bkmk_create();
    if (!b){ fprintf(stderr, "alloc failed\n"); return 1; }

    if (bkmk_count(b) != 0){ fprintf(stderr, "[init] count should be 0\n"); fails++; }
    if (bkmk_has(b, 0)){ fprintf(stderr, "[init] no bookmark yet\n"); fails++; }

    /* toggle three distinct lines out of order -> sorted + unique */
    bkmk_toggle(b, 10);
    bkmk_toggle(b, 3);
    bkmk_toggle(b, 25);
    if (bkmk_count(b) != 3){ fprintf(stderr, "[add] expected 3, got %d\n", bkmk_count(b)); fails++; }
    if (!bkmk_has(b, 3) || !bkmk_has(b, 10) || !bkmk_has(b, 25)){ fprintf(stderr, "[add] missing member\n"); fails++; }
    /* toggling an existing line removes it (idempotent uniqueness) */
    bkmk_toggle(b, 10);
    if (bkmk_count(b) != 2){ fprintf(stderr, "[del] expected 2, got %d\n", bkmk_count(b)); fails++; }
    if (bkmk_has(b, 10)){ fprintf(stderr, "[del] 10 should be gone\n"); fails++; }
    /* re-add 10 -> back to 3 */
    bkmk_toggle(b, 10);
    if (bkmk_count(b) != 3){ fprintf(stderr, "[readd] expected 3, got %d\n", bkmk_count(b)); fails++; }

    /* jump next from line 0 -> first (3); from 3 -> 10; from 4 -> 10 */
    if (bkmk_jump(b, 0, +1) != 3){ fprintf(stderr, "[next] 0->3 (got %d)\n", bkmk_jump(b,0,+1)); fails++; }
    if (bkmk_jump(b, 3, +1) != 10){ fprintf(stderr, "[next] 3->10 (got %d)\n", bkmk_jump(b,3,+1)); fails++; }
    if (bkmk_jump(b, 4, +1) != 10){ fprintf(stderr, "[next] 4->10 (got %d)\n", bkmk_jump(b,4,+1)); fails++; }
    /* jump prev */
    if (bkmk_jump(b, 30, -1) != 25){ fprintf(stderr, "[prev] 30->25 (got %d)\n", bkmk_jump(b,30,-1)); fails++; }
    if (bkmk_jump(b, 25, -1) != 10){ fprintf(stderr, "[prev] 25->10 (got %d)\n", bkmk_jump(b,25,-1)); fails++; }
    /* from before first -> prev returns -1 */
    if (bkmk_jump(b, 2, -1) != -1){ fprintf(stderr, "[prev] 2->-1 (got %d)\n", bkmk_jump(b,2,-1)); fails++; }
    /* empty set: remove the remaining three */
    bkmk_toggle(b, 3); bkmk_toggle(b, 10); bkmk_toggle(b, 25);
    if (bkmk_count(b) != 0){ fprintf(stderr, "[clear] expected 0\n"); fails++; }
    if (bkmk_jump(b, 0, +1) != -1){ fprintf(stderr, "[empty] jump -1\n"); fails++; }

    bkmk_destroy(b);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: bkmk (toggle, sorted-unique, has, jump next/prev, empty)\n");
    return 0;
}
