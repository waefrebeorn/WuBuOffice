/* test_wubutui.c -- pure TUI core: screen buffer, word wrap, key decode, draw.
 * No TTY required: everything here exercises the pure render/input path. */
#include "screen.h"
#include "draw.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

/* Return the y-th line (0-based) of a screen dump into `buf`. */
static void dump_line(const char *dump, size_t y, char *buf, size_t cap) {
    const char *p = dump;
    for (size_t i = 0; i < y && p; i++) { p = strchr(p, '\n'); if (p) p++; }
    buf[0] = 0;
    if (!p) return;
    const char *e = strchr(p, '\n');
    size_t l = e ? (size_t)(e - p) : strlen(p);
    if (l >= cap) l = cap - 1;
    memcpy(buf, p, l); buf[l] = 0;
}

int main(void) {
    /* ---------- screen basics ---------- */
    {
        TuiScreen *s = tui_screen_create(10, 3);
        CK(s != NULL, "create 10x3");
        CK(tui_screen_width(s) == 10 && tui_screen_height(s) == 3, "dims");
        tui_screen_put(s, 0, 0, 'A', TUI_ATTR_NONE);
        tui_screen_put(s, 9, 2, 'Z', TUI_ATTR_BOLD);
        CK(tui_screen_char(s, 0, 0) == 'A', "put/get A");
        CK(tui_screen_char(s, 9, 2) == 'Z', "put/get Z corner");
        CK(tui_screen_attr(s, 9, 2) == TUI_ATTR_BOLD, "attr stored");
        /* out of range is ignored / space */
        tui_screen_put(s, 99, 99, 'X', TUI_ATTR_NONE);
        CK(tui_screen_char(s, 99, 99) == ' ', "oob read is space");
        /* control chars become space */
        tui_screen_put(s, 1, 0, '\t', TUI_ATTR_NONE);
        CK(tui_screen_char(s, 1, 0) == ' ', "control char -> space");
        tui_screen_free(s);
        CK(tui_screen_create(0, 5) == NULL, "reject zero width");
    }

    /* ---------- text + dump ---------- */
    {
        TuiScreen *s = tui_screen_create(8, 2);
        tui_text(s, 0, 0, "Hello", TUI_ATTR_NONE);
        char *d = tui_screen_dump(s);
        CK(d != NULL, "dump not null");
        char line[64];
        dump_line(d, 0, line, sizeof line);
        CK(strncmp(line, "Hello   ", 8) == 0, "row0 = 'Hello   '");
        CK(strlen(line) == 8, "row width padded to 8");
        free(d);
        tui_screen_free(s);
    }

    /* ---------- box ---------- */
    {
        TuiScreen *s = tui_screen_create(5, 3);
        tui_box(s, 0, 0, 5, 3, TUI_ATTR_NONE);
        CK(tui_screen_char(s, 0, 0) == '+', "box corner TL");
        CK(tui_screen_char(s, 4, 0) == '+', "box corner TR");
        CK(tui_screen_char(s, 0, 2) == '+', "box corner BL");
        CK(tui_screen_char(s, 2, 0) == '-', "box top edge");
        CK(tui_screen_char(s, 0, 1) == '|', "box left edge");
        tui_screen_free(s);
    }

    /* ---------- word wrap ---------- */
    {
        size_t n = 0;
        char **w = tui_wrap("the quick brown fox", 9, &n);
        CK(w != NULL, "wrap returns lines");
        /* "the quick"(9) / "brown fox"(9) */
        CK(n == 2, "wrap 'the quick brown fox'@9 -> 2 lines");
        if (n >= 2) {
            CK(strcmp(w[0], "the quick") == 0, "line0 = 'the quick'");
            CK(strcmp(w[1], "brown fox") == 0, "line1 = 'brown fox'");
        }
        tui_wrap_free(w, n);
    }
    {
        /* long word hard-break */
        size_t n = 0;
        char **w = tui_wrap("supercalifragilistic", 5, &n);
        CK(n == 4, "hard-break 20-char word @5 -> 4 lines");
        if (n >= 1) CK(strcmp(w[0], "super") == 0, "hard-break first chunk");
        tui_wrap_free(w, n);
    }
    {
        /* explicit newlines force breaks; blank line preserved */
        size_t n = 0;
        char **w = tui_wrap("a\n\nb", 10, &n);
        CK(n == 3, "newlines -> 3 lines (a, blank, b)");
        if (n >= 3) {
            CK(strcmp(w[0], "a") == 0, "nl line0 'a'");
            CK(strcmp(w[1], "") == 0, "nl line1 blank");
            CK(strcmp(w[2], "b") == 0, "nl line2 'b'");
        }
        tui_wrap_free(w, n);
    }

    /* ---------- wrapped draw + scroll ---------- */
    {
        TuiScreen *s = tui_screen_create(9, 1);
        /* draw the 2-wrap text but scrolled to line 1 -> shows 'brown fox' */
        size_t total = tui_text_wrapped(s, 0, 0, 9, 1, "the quick brown fox", 1, TUI_ATTR_NONE);
        CK(total == 2, "wrapped draw reports 2 total lines");
        char *d = tui_screen_dump(s);
        char line[64]; dump_line(d, 0, line, sizeof line);
        CK(strncmp(line, "brown fox", 9) == 0, "scroll=1 shows second line");
        free(d);
        tui_screen_free(s);
    }

    /* ---------- key decode ---------- */
    {
        TuiKey k;
        CK(tui_key_decode("a", 1, &k) == 1 && k.type == TUI_KEY_CHAR && k.ch == 'a', "char 'a'");
        CK(tui_key_decode("\r", 1, &k) == 1 && k.type == TUI_KEY_ENTER, "enter");
        CK(tui_key_decode("\x7f", 1, &k) == 1 && k.type == TUI_KEY_BACKSPACE, "backspace");
        CK(tui_key_decode("\x1b[A", 3, &k) == 3 && k.type == TUI_KEY_UP, "arrow up CSI");
        CK(tui_key_decode("\x1b[B", 3, &k) == 3 && k.type == TUI_KEY_DOWN, "arrow down CSI");
        CK(tui_key_decode("\x1b[C", 3, &k) == 3 && k.type == TUI_KEY_RIGHT, "arrow right");
        CK(tui_key_decode("\x1b[D", 3, &k) == 3 && k.type == TUI_KEY_LEFT, "arrow left");
        CK(tui_key_decode("\x1b[5~", 4, &k) == 4 && k.type == TUI_KEY_PAGE_UP, "pgup");
        CK(tui_key_decode("\x1b[6~", 4, &k) == 4 && k.type == TUI_KEY_PAGE_DOWN, "pgdn");
        CK(tui_key_decode("\x1b[H", 3, &k) == 3 && k.type == TUI_KEY_HOME, "home CSI");
        CK(tui_key_decode("\x1b[3~", 4, &k) == 4 && k.type == TUI_KEY_DELETE, "delete");
        CK(tui_key_decode("\x1bOA", 3, &k) == 3 && k.type == TUI_KEY_UP, "SS3 up");
        CK(tui_key_decode("\x1b", 1, &k) == 1 && k.type == TUI_KEY_ESC, "lone ESC");
        /* incomplete: partial CSI consumes nothing */
        CK(tui_key_decode("\x1b[", 2, &k) == 0 && k.type == TUI_KEY_INCOMPLETE, "incomplete CSI");
        /* multi-key buffer: consume one at a time */
        size_t u = tui_key_decode("ab", 2, &k);
        CK(u == 1 && k.ch == 'a', "multi-buffer consumes one");
    }

    /* ---------- render (ANSI) sanity + diff ---------- */
    {
        TuiScreen *a = tui_screen_create(3, 1);
        tui_text(a, 0, 0, "abc", TUI_ATTR_NONE);
        size_t len = 0;
        char *r = tui_screen_render(a, NULL, &len);
        CK(r != NULL && len > 0, "full render non-empty");
        CK(strstr(r, "abc") != NULL, "render contains 'abc'");
        free(r);
        /* diff render: identical -> no cell payload change (still may have home+sgr) */
        TuiScreen *b = tui_screen_create(3, 1);
        tui_screen_copy(b, a);
        char *rd = tui_screen_render(a, b, &len);
        CK(rd != NULL && strstr(rd, "abc") == NULL, "diff of identical omits cells");
        free(rd);
        tui_screen_free(a);
        tui_screen_free(b);
    }

    if (fails) { printf("WUBUTUI TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUTUI TESTS PASSED\n");
    return 0;
}
