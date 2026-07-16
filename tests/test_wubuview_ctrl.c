/* test_wubuview_ctrl.c -- pure interaction controller (viewport + tabs). */
#include "controller.h"
#include "input.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static TuiKey ev(const char *s) {
    TuiKey k;
    k.type = TUI_KEY_NONE; k.ch = 0;
    k.mouse_action = TUI_MOUSE_PRESS; k.mouse_button = TUI_MBTN_NONE;
    k.mouse_x = k.mouse_y = 0; k.mouse_shift = k.mouse_alt = k.mouse_ctrl = 0;
    tui_key_decode(s, strlen(s), &k);
    return k;
}
static TuiKey ch(char c) { TuiKey k; k.type = TUI_KEY_CHAR; k.ch = c; return k; }

static void feedc(VState *st, char c) { TuiKey k = ch(c); vctrl_handle(st, &k); }
static void feeds(VState *st, const char *s) { TuiKey k = ev(s); vctrl_handle(st, &k); }

int main(void) {
    VState st;
    /* layout: row0 header, row1 tabbar, footer; body_h = H-3 */
    vctrl_init(&st, 80, 24);
    CK(st.body_h == 21, "body_h = H-3 = 21");
    int tid = vctrl_open(&st, "doc.txt", 100);
    CK(tid == 0, "first tab index 0");
    CK(vctrl_max_scroll(&st) == 79, "max_scroll = 100-21 = 79");
    CK(st.tabs[st.tab_active].scroll == 0, "starts at top");

    /* keyboard scroll on the active tab */
    feedc(&st, 'j');
    CK(st.tabs[st.tab_active].scroll == 1, "j -> scroll 1");
    feedc(&st, 'G');
    CK(st.tabs[st.tab_active].scroll == 79, "G -> bottom (max)");
    feedc(&st, 'k');
    CK(st.tabs[st.tab_active].scroll == 78, "k -> scroll 78");
    feedc(&st, 'g');
    CK(st.tabs[st.tab_active].scroll == 0, "g -> top");
    feedc(&st, ' ');
    CK(st.tabs[st.tab_active].scroll == 20, "space -> page down (body_h-1)");
    feeds(&st, "\x1b[B");  /* down */
    CK(st.tabs[st.tab_active].scroll == 21, "down arrow -> 21");
    feeds(&st, "\x1b[5~"); /* pgup */
    CK(st.tabs[st.tab_active].scroll == 1, "pgup -> 1");
    feeds(&st, "\x1b[6~"); /* pgdn */
    CK(st.tabs[st.tab_active].scroll == 21, "pgdn -> 21");
    feeds(&st, "\x1b[H");  /* home */
    CK(st.tabs[st.tab_active].scroll == 0, "home -> top");

    feedc(&st, 'q');
    CK(st.running == 0, "q -> not running");
    feedc(&st, '\x1b');
    CK(st.running == 0, "esc also quits");

    /* mouse: wheel */
    vctrl_init(&st, 80, 24);
    vctrl_open(&st, "a", 100);
    feeds(&st, "\x1b[<64;5;5M"); /* wheel up */
    CK(st.tabs[st.tab_active].scroll == 0, "wheel up at top stays 0");
    feedc(&st, 'G');
    feeds(&st, "\x1b[<65;5;5M"); /* wheel down */
    CK(st.tabs[st.tab_active].scroll == 79, "wheel down at bottom stays max");
    feeds(&st, "\x1b[<64;5;5M"); /* wheel up x1 */
    CK(st.tabs[st.tab_active].scroll == 76, "wheel up -> 76");
    feeds(&st, "\x1b[<65;5;5M"); /* wheel down x1 */
    CK(st.tabs[st.tab_active].scroll == 79, "wheel down -> 79");

    /* mouse: scrollbar click jumps */
    vctrl_init(&st, 80, 24);
    vctrl_open(&st, "a", 100);
    TuiKey click = ev("\x1b[<0;80;12M");  /* x=79, y=11 */
    CK(click.mouse_x == 79 && click.mouse_y == 11, "scrollbar click coords decoded");
    VBtn hit = vctrl_handle(&st, &click);
    CK(hit == VB__COUNT, "scrollbar click is not a footer button");
    CK(st.tabs[st.tab_active].scroll == 39, "mid-track click -> ~mid scroll (39)");
    CK(st.dragging == 1, "click sets dragging");

    TuiKey drag = ev("\x1b[<32;80;2M");  /* drag at y=1 */
    vctrl_handle(&st, &drag);
    CK(st.tabs[st.tab_active].scroll == 0, "drag to top -> scroll 0");
    TuiKey rel = ev("\x1b[<0;80;2m");
    vctrl_handle(&st, &rel);
    CK(st.dragging == 0, "release clears dragging");

    /* mouse: footer buttons */
    vctrl_init(&st, 80, 24);
    vctrl_open(&st, "a", 100);
    VBtn b = vctrl_button_at(&st, st.btn_x[VB_BOT], st.footer_y);
    CK(b == VB_BOT, "button_at finds Bot in footer");
    char seq[32];
    snprintf(seq, sizeof seq, "\x1b[<0;%zu;%zuM", st.btn_x[VB_BOT] + 1, st.footer_y + 1);
    TuiKey __kb = ev(seq); VBtn clicked = vctrl_handle(&st, &__kb);
    CK(clicked == VB_BOT, "clicking Bot returns VB_BOT");
    CK(st.tabs[st.tab_active].scroll == 79, "Bot click -> bottom");
    char seq2[32];
    snprintf(seq2, sizeof seq2, "\x1b[<0;%zu;%zuM", st.btn_x[VB_TOP] + 1, st.footer_y + 1);
    TuiKey __k2 = ev(seq2); VBtn c2 = vctrl_handle(&st, &__k2);
    CK(c2 == VB_TOP && st.tabs[st.tab_active].scroll == 0, "Top click -> top");
    char seq3[32];
    snprintf(seq3, sizeof seq3, "\x1b[<0;%zu;%zuM", st.btn_x[VB_QUIT] + 1, st.footer_y + 1);
    TuiKey __k3 = ev(seq3); VBtn c3 = vctrl_handle(&st, &__k3);
    CK(c3 == VB_QUIT && st.running == 0, "Quit click -> not running");

    /* multi-tab: open, switch, per-tab scroll */
    vctrl_init(&st, 80, 24);
    int t0 = vctrl_open(&st, "notes.md", 100);
    int t1 = vctrl_open(&st, "todo.txt", 50);
    CK(t0 == 0 && t1 == 1, "two tabs opened");
    CK(st.tab_n == 2 && st.tab_active == 1, "active is the last opened");
    feedc(&st, 'G');  /* scroll tab1 to bottom */
    CK(st.tabs[1].scroll == 29, "tab1 bottom (50-21)");
    size_t sw = vctrl_switch(&st, -1);
    CK(sw == 0 && st.tab_active == 0, "switch to previous tab");
    CK(st.tabs[0].scroll == 0, "tab0 scroll preserved");
    vctrl_switch(&st, +1);
    CK(st.tab_active == 1, "switch forward stays 1");
    vctrl_switch(&st, +1);
    CK(st.tab_active == 0, "switch forward wraps to 0");
    size_t a = vctrl_close(&st, 0);
    CK(a == 0 && st.tab_n == 1, "close tab0 -> 1 tab left, active 0");

    /* resize keeps scroll clamped */
    vctrl_init(&st, 80, 24);
    vctrl_open(&st, "a", 100);
    feedc(&st, 'G');
    CK(st.tabs[st.tab_active].scroll == 79, "at bottom before resize");
    vctrl_resize(&st, 80, 40, 100);   /* taller: body_h 37 */
    CK(st.body_h == 37, "resize body_h=37");
    CK(vctrl_max_scroll(&st) == 63, "resize max_scroll=63");
    CK(st.tabs[st.tab_active].scroll == 63, "scroll clamped to new max on resize");

    /* command palette (Ctrl+K) */
    vctrl_palette_open(&st);
    CK(st.palette == 1, "palette opens");
    /* type 'bo' -> matches 'bottom' */
    feedc(&st, 'b'); feedc(&st, 'o');
    CK(st.pal_len == 2 && strncmp(st.pal_buf, "bo", 2) == 0, "palette buffers 'bo'");
    /* Enter runs 'bottom' -> scroll to max */
    feedc(&st, 0x0d);
    CK(st.palette == 0, "palette closes after run");
    CK(st.tabs[st.tab_active].scroll == 63, "palette 'bottom' -> max scroll");
    /* palette Esc cancels without action */
    vctrl_palette_open(&st);
    feedc(&st, 'q'); feedc(&st, 0x1b);
    CK(st.palette == 0, "palette Esc cancels");
    CK(st.tabs[st.tab_active].scroll == 63, "Esc leaves scroll unchanged");
    /* palette 'top' from a scrolled position */
    feedc(&st, 'G');  /* bottom */
    vctrl_palette_open(&st); feedc(&st, 't'); feedc(&st, 'o'); feedc(&st, 0x0d);
    CK(st.tabs[st.tab_active].scroll == 0, "palette 'top' -> scroll 0");

    if (fails) { printf("WUBUVIEW CTRL TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUVIEW CTRL TESTS PASSED\n");
    return 0;
}
