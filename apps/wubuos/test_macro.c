/* test_macro.c -- headless unit test for the extracted opaque Macro engine.
 * Verifies record (chars/return/backspace), toggle, count, and playback via
 * the op callback. Pure logic, no GUI.
 */
#include "macro.h"
#include <stdio.h>
#include <string.h>

static int g_ncalls;
static int g_last_op;
static unsigned char g_last_ch;
static int g_seq[64];
static int g_seqn;

static void cb(int op, unsigned char ch, void *ctx){
    (void)ctx;
    g_ncalls++;
    g_last_op = op; g_last_ch = ch;
    if (g_seqn < 64) g_seq[g_seqn++] = op;
}

int main(void){
    int fails = 0;
    Macro *m = macro_create();
    if (!m){ fprintf(stderr, "alloc\n"); return 1; }

    /* not recording -> record is a no-op */
    macro_record(m, MACRO_OP_CHAR, 'x');
    if (macro_count(m) != 0){ fprintf(stderr, "[no-op when idle]\n"); fails++; }

    /* start recording */
    if (!macro_toggle_rec(m)){ fprintf(stderr, "[start rec]\n"); fails++; }
    macro_record(m, MACRO_OP_CHAR, 'a');
    macro_record(m, MACRO_OP_CHAR, 'b');
    macro_record(m, MACRO_OP_RETURN, 0);
    macro_record(m, MACRO_OP_BACKSPACE, 0);
    macro_record(m, MACRO_OP_CHAR, 'c');
    if (macro_count(m) != 5){ fprintf(stderr, "[count] got %d want 5\n", macro_count(m)); fails++; }

    /* stop recording */
    if (macro_toggle_rec(m)){ fprintf(stderr, "[stop rec]\n"); fails++; }

    /* playback: callback must see 5 ops in order: CHAR,CHAR,RETURN,BACKSPACE,CHAR */
    g_ncalls = 0; g_seqn = 0;
    macro_play(m, cb, NULL);
    if (g_ncalls != 5){ fprintf(stderr, "[play count] got %d\n", g_ncalls); fails++; }
    int want[5] = { MACRO_OP_CHAR, MACRO_OP_CHAR, MACRO_OP_RETURN, MACRO_OP_BACKSPACE, MACRO_OP_CHAR };
    for (int i=0;i<5;i++) if (g_seq[i] != want[i]){ fprintf(stderr, "[play seq %d] got %d want %d\n", i, g_seq[i], want[i]); fails++; }
    /* last char op should carry 'c' */
    if (!(g_last_op==MACRO_OP_CHAR && g_last_ch=='c')){ fprintf(stderr, "[last char] op=%d ch=%c\n", g_last_op, g_last_ch); fails++; }

    /* playback while NOT recording must not toggle recording state */
    if (macro_recording(m)){ fprintf(stderr, "[rec must stay off]\n"); fails++; }

    /* re-record clears buffer */
    macro_toggle_rec(m);
    macro_record(m, MACRO_OP_CHAR, 'z');
    if (macro_count(m) != 1){ fprintf(stderr, "[clear on re-record] got %d\n", macro_count(m)); fails++; }

    macro_destroy(m);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: macro (record chars/return/backspace, toggle, count, playback seq)\n");
    return 0;
}
