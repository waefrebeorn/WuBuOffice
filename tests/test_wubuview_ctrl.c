/* test_wubuview_ctrl.c -- pure interaction controller.
 *
 * This tests the REAL event->state mapping used by the interactive viewer
 * (apps/wubuview/controller.c) -- not a re-implementation. Feed keys/mouse
 * from byte sequences and assert scroll/quit transitions. No TTY needed. */
#include "controller.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

/* decode one event from a literal escape/byte string */
static TuiKey ev(const char *s) {
    TuiKey k;
    /* prime unused fields */
    k.type = TUI_KEY_NONE; k.ch = 0;
    k.mouse_action = TUI_MOUSE_PRESS; k.mouse_button = TUI_MBTN_NONE;
    k.mouse_x = k.mouse_y = 0; k.mouse_shift = k.mouse_alt = k.mouse_ctrl = 0;
    tui_key_decode(s, strlen(s), &k);
    return k;
}
static TuiKey ch(char c) { TuiKey k; k.type = TUI_KEY_CHAR; k.ch = c; return k; }

/* feed a char event into the controller, returning nothing (updates st) */
static void feedc(VState *st, char c) { TuiKey k = ch(c); vctrl_handle(st, &k); }
/* feed a decoded event from a literal string */
static void feeds(VState *st, const char *s) { TuiKey k = ev(s); vctrl_handle(st, &k); }

int main(void) {
    VState st;
    /* 100-line doc in a 24-row terminal (body_h = 22) */
    vctrl_init(&st, 80, 24, 100);
    CK(st.body_h == 22, "body_h = H-2 = 22");
    CK(vctrl_max_scroll(&st) == 78, "max_scroll = 100-22 = 78");
    CK(st.scroll == 0, "starts at top");

    /* keyboard scroll */
    feedc(&st, 'j');
    CK(st.scroll == 1, "j -> scroll 1");
    feedc(&st, 'G');
    CK(st.scroll == 78, "G -> bottom (max)");
    feedc(&st, 'k');
    CK(st.scroll == 77, "k -> scroll 77");
    feedc(&st, 'g');
    CK(st.scroll == 0, "g -> top");
    feedc(&st, ' ');
    CK(st.scroll == 22, "space -> page down (body_h)");
    feeds(&st, "\x1b[B");  /* down */
    CK(st.scroll == 23, "down arrow -> 23");
    feeds(&st, "\x1b[5~"); /* pgup */
    CK(st.scroll == 1, "pgup -> 1");
    feeds(&st, "\x1b[6~"); /* pgdn */
    CK(st.scroll == 23, "pgdn -> 23");
    feeds(&st, "\x1b[H");  /* home */
    CK(st.scroll == 0, "home -> top");

    /* quit via 'q' */
    feedc(&st, 'q');
    CK(st.running == 0, "q -> not running");
    feedc(&st, '\x1b'); /* esc */
    CK(st.running == 0, "esc also quits");

    /* --- mouse: wheel --- */
    vctrl_init(&st, 80, 24, 100);
    feeds(&st, "\x1b[<64;5;5M"); /* wheel up */
    CK(st.scroll == 0, "wheel up at top stays 0");
    feedc(&st, 'G');
    feeds(&st, "\x1b[<65;5;5M"); /* wheel down */
    CK(st.scroll == 78, "wheel down at bottom stays max");
    feeds(&st, "\x1b[<64;5;5M"); /* wheel up x1 */
    CK(st.scroll == 75, "wheel up -> 75");
    feeds(&st, "\x1b[<65;5;5M"); /* wheel down x1 */
    CK(st.scroll == 78, "wheel down -> 78");

    /* --- mouse: scrollbar click jumps --- */
    vctrl_init(&st, 80, 24, 100);
    TuiKey click = ev("\x1b[<0;80;12M");  /* x=80(0-based 79), y=12(0-based 11) */
    CK(click.mouse_x == 79 && click.mouse_y == 11, "scrollbar click coords decoded");
    VBtn hit = vctrl_handle(&st, &click);
    CK(hit == VB__COUNT, "scrollbar click is not a footer button");
    CK(st.scroll == 37, "mid-track click -> ~mid scroll (37)");
    CK(st.dragging == 1, "click sets dragging");

    /* drag the thumb to the top while held */
    TuiKey drag = ev("\x1b[<32;80;2M");  /* drag at y=2 (0-based 1) */
    vctrl_handle(&st, &drag);
    CK(st.scroll == 0, "drag to top -> scroll 0");
    /* release */
    TuiKey rel = ev("\x1b[<0;80;2m");
    vctrl_handle(&st, &rel);
    CK(st.dragging == 0, "release clears dragging");

    /* --- mouse: footer buttons --- */
    vctrl_init(&st, 80, 24, 100);
    /* the "Bot" button lives somewhere in the footer row (23). Click its column. */
    VBtn b = vctrl_button_at(&st, st.btn_x[VB_BOT], st.footer_y);
    CK(b == VB_BOT, "button_at finds Bot in footer");
    /* build a left-press event at the Bot button cell */
    char seq[32];
    snprintf(seq, sizeof seq, "\x1b[<0;%zu;%zuM", st.btn_x[VB_BOT] + 1, st.footer_y + 1);
    TuiKey bc = ev(seq);
    VBtn clicked = vctrl_handle(&st, &bc);
    CK(clicked == VB_BOT, "clicking Bot returns VB_BOT");
    CK(st.scroll == 78, "Bot click -> bottom");
    /* Top */
    char seq2[32];
    snprintf(seq2, sizeof seq2, "\x1b[<0;%zu;%zuM", st.btn_x[VB_TOP] + 1, st.footer_y + 1);
    TuiKey k2 = ev(seq2);
    VBtn c2 = vctrl_handle(&st, &k2);
    CK(c2 == VB_TOP && st.scroll == 0, "Top click -> top");
    /* Quit */
    char seq3[32];
    snprintf(seq3, sizeof seq3, "\x1b[<0;%zu;%zuM", st.btn_x[VB_QUIT] + 1, st.footer_y + 1);
    TuiKey k3 = ev(seq3);
    VBtn c3 = vctrl_handle(&st, &k3);
    CK(c3 == VB_QUIT && st.running == 0, "Quit click -> not running");

    /* --- resize keeps scroll clamped --- */
    vctrl_init(&st, 80, 24, 100);
    feedc(&st, 'G');     /* bottom */
    CK(st.scroll == 78, "at bottom before resize");
    vctrl_resize(&st, 80, 40, 100);        /* taller: body_h 38 */
    CK(st.body_h == 38, "resize body_h=38");
    CK(vctrl_max_scroll(&st) == 62, "resize max_scroll=62");
    CK(st.scroll == 62, "scroll clamped to new max on resize");
    if (fails) { printf("WUBUVIEW CTRL TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUVIEW CTRL TESTS PASSED\n");
    return 0;
}
