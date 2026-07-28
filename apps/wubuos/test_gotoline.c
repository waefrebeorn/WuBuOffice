/* test_gotoline.c -- headless unit test for the extracted opaque GotoLine
 * engine. Verifies open/active, digit capture, backspace, commit (returns
 * 1-based line), and Esc cancel. Pure logic, no GUI.
 */
#include "gotoline.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int feed(GotoLine *g, const char *s){
    for (const char *p=s; *p; p++){
        int r = gotoline_key(g, (int)(unsigned char)*p);
        if (r) return r;          /* 1=commit, 2=cancel */
    }
    return 0;
}

int main(void){
    int fails = 0;
    GotoLine *g = gotoline_create();
    if (!g){ fprintf(stderr, "alloc\n"); return 1; }

    if (gotoline_active(g)){ fprintf(stderr, "[not closed initially]\n"); fails++; }
    if (gotoline_buf(g) == NULL){ fprintf(stderr, "[buf null]\n"); fails++; }

    gotoline_open(g);
    if (!gotoline_active(g)){ fprintf(stderr, "[open]\n"); fails++; }
    if (strlen(gotoline_buf(g)) != 0){ fprintf(stderr, "[buf not empty on open]\n"); fails++; }

    /* type "12" then backspace -> "1" */
    feed(g, "12");
    if (strcmp(gotoline_buf(g), "12") != 0){ fprintf(stderr, "[buf '12'] got '%s'\n", gotoline_buf(g)); fails++; }
    gotoline_key(g, 8);  /* backspace */
    if (strcmp(gotoline_buf(g), "1") != 0){ fprintf(stderr, "[backspace] got '%s'\n", gotoline_buf(g)); fails++; }

    /* commit: returns 1-based line */
    int ln = gotoline_commit(g);
    if (ln != 1){ fprintf(stderr, "[commit] got %d want 1\n", ln); fails++; }
    if (gotoline_active(g)){ fprintf(stderr, "[closed after commit]\n"); fails++; }

    /* re-open, type "42", commit -> 42 */
    gotoline_open(g);
    feed(g, "42");
    ln = gotoline_commit(g);
    if (ln != 42){ fprintf(stderr, "[commit 42] got %d\n", ln); fails++; }

    /* open, type nothing, commit -> 0 (invalid) */
    gotoline_open(g);
    ln = gotoline_commit(g);
    if (ln != 0){ fprintf(stderr, "[commit empty] got %d\n", ln); fails++; }

    /* Esc cancels (returns 2) and closes */
    gotoline_open(g);
    feed(g, "9");
    int r = gotoline_key(g, 27);   /* Esc */
    if (r != 2){ fprintf(stderr, "[esc] ret %d\n", r); fails++; }
    if (gotoline_active(g)){ fprintf(stderr, "[esc closes]\n"); fails++; }

    /* any non-digit non-control key dismisses (returns 2) */
    gotoline_open(g);
    feed(g, "5");
    r = gotoline_key(g, 'a');      /* letter -> dismiss */
    if (r != 2){ fprintf(stderr, "[dismiss on letter] ret %d\n", r); fails++; }
    if (gotoline_active(g)){ fprintf(stderr, "[dismiss closes]\n"); fails++; }

    gotoline_destroy(g);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: gotoline (open/active, digit capture, backspace, commit, esc, dismiss)\n");
    return 0;
}
