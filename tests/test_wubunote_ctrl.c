/* test_wubunote_ctrl.c -- pure wubunote interaction controller.
 *
 * Tests the REAL event->state mapping in apps/wubunote/note_controller.c
 * (tabs + editing + prompt/palette), not a re-implementation. No TTY.
 */
#include "note_controller.h"
#include "input.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static TuiKey ch(char c) { TuiKey k; k.type = TUI_KEY_CHAR; k.ch = c; return k; }
static TuiKey ev(const char *s) __attribute__((unused));
static TuiKey ev(const char *s) {
    TuiKey k; k.type = TUI_KEY_NONE; k.ch = 0;
    k.mouse_action = TUI_MOUSE_PRESS; k.mouse_button = TUI_MBTN_NONE;
    k.mouse_x = k.mouse_y = 0; k.mouse_shift = k.mouse_alt = k.mouse_ctrl = 0;
    tui_key_decode(s, strlen(s), &k);
    return k;
}
static void feedc(NState *st, char c) { TuiKey k = ch(c); nctrl_handle(st, &k, NULL); }
/* type a literal string of chars */
static void types(NState *st, const char *s) { for (; *s; s++) feedc(st, *s); }
/* commit a prompt with Enter; out_path captured if set */
static void commit(NState *st, char **out) {
    TuiKey k = ch(0x0d);  /* Enter */
    nctrl_handle(st, &k, out);
}

int main(void) {
    NState st;
    nctrl_init(&st, 80, 24);
    CK(st.running == 1, "starts running");
    CK(st.tab_n == 0, "no tabs yet");

    /* new empty tab */
    nctrl_new(&st);
    CK(st.tab_n == 1 && st.tab_active == 0, "new -> 1 tab, active 0");
    CK(strcmp(st.tabs[0].path, "untitled-1") == 0, "untitled default name");

    /* type + backspace */
    types(&st, "hello");
    CK(strcmp(edit_line(st.tabs[st.tab_active].buf, 0), "hello") == 0, "typed hello");
    feedc(&st, 0x7f);  /* backspace */
    CK(strcmp(edit_line(st.tabs[st.tab_active].buf, 0), "hell") == 0, "backspace -> hell");
    /* enter splits the line */
    feedc(&st, 0x0d);
    CK(edit_line_count(st.tabs[st.tab_active].buf) == 2, "enter -> 2 lines");
    CK(strcmp(edit_line(st.tabs[st.tab_active].buf, 0), "hell") == 0, "line0 = hell");

    /* open a second tab; active switches to it */
    nctrl_open(&st, "notes.md");
    CK(st.tab_n == 2 && st.tab_active == 1, "open -> 2 tabs, active new");
    CK(strcmp(st.tabs[1].path, "notes.md") == 0, "tab path set");
    /* editing the active tab doesn't touch the first */
    types(&st, "abc");
    CK(strcmp(edit_line(st.tabs[st.tab_active].buf, 0), "abc") == 0, "tab1 has abc");
    CK(strcmp(edit_line(st.tabs[0].buf, 0), "hell") == 0, "tab0 untouched");

    /* Ctrl+B / Ctrl+E switch tabs */
    TuiKey cb = ch(0x02); nctrl_handle(&st, &cb, NULL);  /* prev */
    CK(st.tab_active == 0, "Ctrl+B -> tab0");
    TuiKey ce = ch(0x05); nctrl_handle(&st, &ce, NULL);  /* next */
    CK(st.tab_active == 1, "Ctrl+E -> tab1");

    /* wrap toggle (Ctrl+W) */
    int w0 = st.wrap;
    TuiKey cw = ch(0x17); nctrl_handle(&st, &cw, NULL);
    CK(st.wrap != w0, "Ctrl+W toggles wrap");

    /* find: type a doc then search */
    nctrl_new(&st);  /* tab2 */
    types(&st, "alpha beta gamma alpha");
    nctrl_prompt_find(&st);
    CK(st.prompt == NPMT_FIND, "find prompt open");
    types(&st, "alpha");
    commit(&st, NULL);
    CK(edit_cursor_row(nctrl_active_buf(&st)) == 0, "find -> first alpha on line 0");

    /* goto line (Ctrl+G) */
    nctrl_prompt_goto(&st);
    CK(st.prompt == NPMT_GOTO, "goto prompt open");
    types(&st, "1");
    commit(&st, NULL);
    CK(edit_cursor_row(st.tabs[st.tab_active].buf) == 0, "goto 1 -> row 0");

    /* save-as: prompt returns a malloc'd path via out_path */
    nctrl_prompt_saveas(&st);
    CK(st.prompt == NPMT_SAVEAS, "saveas prompt open");
    types(&st, "/tmp/x.txt");
    char *out = NULL;
    commit(&st, &out);
    CK(out && strcmp(out, "/tmp/x.txt") == 0, "saveas returns path");
    free(out);

    /* command palette (Ctrl+K) */
    TuiKey ck = ch(0x0b); nctrl_handle(&st, &ck, NULL);
    CK(st.prompt == NPMT_CMD, "Ctrl+K opens command palette");
    types(&st, ":w");          /* save command */
    char *out2 = NULL;
    commit(&st, &out2);
    CK(out2 && strcmp(out2, "/tmp/x.txt") == 0, "palette :w saves current path");
    free(out2);

    /* Esc cancels a prompt */
    nctrl_prompt_find(&st);
    TuiKey esc = ch(0x1b); nctrl_handle(&st, &esc, NULL);
    CK(st.prompt == NPMT_NONE, "Esc cancels prompt");

    /* close tabs down to 0 -> quits */
    while (st.tab_n) nctrl_close_active(&st);
    CK(st.running == 0, "closing last tab quits");

    if (fails) { printf("WUBUNOTE CTRL TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUNOTE CTRL TESTS PASSED\n");
    return 0;
}
