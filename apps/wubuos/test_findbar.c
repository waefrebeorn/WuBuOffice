/* test_findbar.c -- headless unit test for the extracted opaque FindBar engine.
 * Verifies find/replace logic independent of the editor view (no SDL/GUI).
 */
#include "findbar.h"
#include "doc.h"      /* WuBuPad piece-table Doc (built into the test target) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static Doc *mk_doc(const char *s){
    Doc *d = doc_create(s);   /* doc_create takes a (possibly NULL) seed */
    return d;
}

int main(void){
    int fails = 0;
    /* seed: "alpha beta alpha gamma alpha" */
    Doc *d = mk_doc("alpha beta alpha gamma alpha");
    FindBar *fb = findbar_create();
    if (!d || !fb){ fprintf(stderr, "alloc failed\n"); return 1; }

    /* --- literal find: three 'alpha' matches --- */
    findbar_set_query(fb, "alpha");
    if (!findbar_next(fb, d, 0)){ fprintf(stderr, "[lit] first match not found\n"); fails++; }
    else {
        int idx=0, tot=0; findbar_counts(fb, &idx, &tot);
        if (tot != 3){ fprintf(stderr, "[lit] expected 3 matches, got %d\n", tot); fails++; }
        if (idx != 1){ fprintf(stderr, "[lit] first idx should be 1, got %d\n", idx); fails++; }
    }
    /* advance through all three */
    int seen = 1; size_t prev = 0;
    while (findbar_next(fb, d, prev+1)){
        int idx=0, tot=0; findbar_counts(fb, &idx, &tot);
        if (idx != seen+1) break;
        size_t s=0,e=0; findbar_match(fb, &s, &e);
        prev = s; seen++;
        if (seen > 3) break;
    }
    if (seen != 3){ fprintf(stderr, "[lit] expected to walk 3 matches, walked %d\n", seen); fails++; }

    /* --- replace one --- */
    findbar_set_query(fb, "alpha");
    findbar_set_replace(fb, "X");
    findbar_next(fb, d, 0);
    findbar_replace_one(fb, d);
    char *t = doc_text(d);
    if (strncmp(t, "X beta", 6) != 0){ fprintf(stderr, "[rep1] got '%s'\n", t); fails++; }
    free(t);

    /* --- replace all remaining 'alpha' -> 'X' --- */
    findbar_next(fb, d, 0);          /* re-find from start (now "X beta alpha gamma alpha") */
    findbar_replace_all(fb, d);
    t = doc_text(d);
    if (strcmp(t, "X beta X gamma X") != 0){ fprintf(stderr, "[repall] got '%s'\n", t); fails++; }
    free(t);

    /* --- regex find --- */
    findbar_set_query(fb, "[bg]amma");
    findbar_set_regex(fb, 1);
    if (!findbar_next(fb, d, 0)){ fprintf(stderr, "[rx] no regex match\n"); fails++; }
    else {
        size_t s=0,e=0; findbar_match(fb, &s, &e);
        t = doc_text(d);
        if (strncmp(t+s, "gamma", e-s) != 0){ fprintf(stderr, "[rx] matched '%s' not 'gamma'\n", t+s); fails++; }
        free(t);
    }

    /* --- clear active + no-match behaviour --- */
    findbar_clear_active(fb);
    if (findbar_active(fb)){ fprintf(stderr, "[clr] still active after clear\n"); fails++; }
    findbar_set_query(fb, "zzz_nomatch_zzz");
    findbar_set_regex(fb, 0);
    if (findbar_next(fb, d, 0)){ fprintf(stderr, "[nomatch] should not match\n"); fails++; }
    if (findbar_active(fb)){ fprintf(stderr, "[nomatch] active after no-match\n"); fails++; }

    findbar_destroy(fb);
    doc_free(d);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: findbar (literal/regex find, replace-one, replace-all, clear, no-match)\n");
    return 0;
}
