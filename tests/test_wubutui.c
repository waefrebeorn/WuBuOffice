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

    /* ---------- mouse decode (SGR 1006 + legacy X10) ---------- */
    {
        TuiKey k;
        /* left press at (x=3,y=2) -> coords are 1-based in the report */
        const char *seq = "\x1b[<0;3;2M";
        size_t u = tui_key_decode(seq, strlen(seq), &k);
        CK(u == strlen(seq) && k.type == TUI_KEY_MOUSE, "sgr left press parsed");
        CK(k.mouse_action == TUI_MOUSE_PRESS, "sgr -> PRESS");
        CK(k.mouse_button == TUI_MBTN_LEFT, "sgr -> LEFT button");
        CK(k.mouse_x == 2 && k.mouse_y == 1, "sgr 1-based coords -> 0-based");

        /* wheel up (cb=64) at (5,4) */
        const char *wup = "\x1b[<64;5;4M";
        tui_key_decode(wup, strlen(wup), &k);
        CK(k.mouse_action == TUI_MOUSE_WHEEL_UP && k.mouse_x == 4 && k.mouse_y == 3,
            "sgr wheel up + coords");

        /* wheel down (cb=65) */
        const char *wdn = "\x1b[<65;1;1M";
        tui_key_decode(wdn, strlen(wdn), &k);
        CK(k.mouse_action == TUI_MOUSE_WHEEL_DOWN, "sgr wheel down");

        /* release: ends with 'm' */
        const char *rel = "\x1b[<0;3;2m";
        tui_key_decode(rel, strlen(rel), &k);
        CK(k.mouse_action == TUI_MOUSE_RELEASE && k.mouse_button == TUI_MBTN_LEFT,
            "sgr release ('m') -> RELEASE+LEFT");

        /* drag (cb=32 = motion, left held at 0) at (10,20) */
        const char *drag = "\x1b[<32;11;21M";
        tui_key_decode(drag, strlen(drag), &k);
        CK(k.mouse_action == TUI_MOUSE_DRAG && k.mouse_x == 10 && k.mouse_y == 20,
            "sgr drag -> DRAG + coords");

        /* shift+left: cb=4 (shift bit) */
        const char *sh = "\x1b[<4;1;1M";
        tui_key_decode(sh, strlen(sh), &k);
        CK(k.mouse_shift == 1 && k.mouse_button == TUI_MBTN_LEFT, "sgr shift modifier flag");

        /* legacy X10: ESC[M bxy, each biased +32. left(0)->32(' '), x=3->35('#'), y=2->34('"') */
        const char *x10 = "\x1b[M #\"";
        size_t xu = tui_key_decode(x10, strlen(x10), &k);
        CK(xu == 6 && k.type == TUI_KEY_MOUSE && k.mouse_action == TUI_MOUSE_PRESS,
            "x10 left press parsed");
        CK(k.mouse_x == 2 && k.mouse_y == 1, "x10 coords decoded");

        /* incomplete: truncated SGR (no terminator yet) */
        const char *inc = "\x1b[<0;3;";
        size_t iu = tui_key_decode(inc, strlen(inc), &k);
        CK(iu == 0 && k.type == TUI_KEY_INCOMPLETE, "incomplete SGR -> INCOMPLETE");

        /* caller buffers partial escapes: feed partial (0 consumed), then the
         * full accumulated buffer decodes cleanly in one shot */
        const char *full = "\x1b[<0;3;2M";
        size_t a = tui_key_decode(full, 5, &k);   /* first 5 bytes: incomplete */
        CK(a == 0 && k.type == TUI_KEY_INCOMPLETE, "partial -> 0 consumed, INCOMPLETE");
        /* caller appends the rest and re-decodes the whole thing */
        size_t fb = tui_key_decode(full, strlen(full), &k);
        CK(fb == strlen(full) && k.type == TUI_KEY_MOUSE, "re-feed full -> MOUSE");
    }

    /* ---------- scrollbar geometry ---------- */
    {
        /* everything fits -> thumb spans the whole track */
        TuiThumb th = tui_scrollbar_thumb(20, 10, 30, 0);
        CK(th.start == 0 && th.len == 20, "fits: thumb == full track");

        /* half content visible at top: thumb is half the track, pinned at top */
        th = tui_scrollbar_thumb(20, 100, 50, 0);
        CK(th.len == 10, "half: thumb = 50% track");
        CK(th.start == 0, "half@top: thumb at top");

        /* at bottom -> thumb pinned to bottom */
        th = tui_scrollbar_thumb(20, 100, 50, 50);
        CK(th.start + th.len == 20, "bottom: thumb pinned to end");

        /* middle scroll -> thumb in the middle */
        th = tui_scrollbar_thumb(20, 100, 50, 25);
        CK(th.start == 5, "mid: thumb centered");

        /* length never exceeds track, never zero when content > viewport */
        th = tui_scrollbar_thumb(7, 1000, 3, 0);
        CK(th.len >= 1 && th.len <= 7, "thumb length within [1, h]");
    }

    /* ---------- scrollbar click maps to scroll ---------- */
    {
        /* 20-row track, 100 lines, 50 visible. Clicking top row -> scroll 0. */
        CK(tui_scrollbar_scroll_at(20, 100, 50, 0) == 0, "click top -> scroll 0");
        /* clicking last row -> scroll max (50) */
        CK(tui_scrollbar_scroll_at(20, 100, 50, 19) == 50, "click bottom -> max");
        /* everything fits -> always 0 */
        CK(tui_scrollbar_scroll_at(20, 10, 30, 0) == 0, "fits -> scroll 0");
    }

    /* ---------- button + hit-test ---------- */
    {
        CK(tui_button_width("Quit") == 8, "button width '[ Quit ]' = 8");
        /* draw a button at (1,1) and confirm its glyphs */
        TuiScreen *s = tui_screen_create(10, 3);
        tui_button(s, 1, 1, "Hi", TUI_ATTR_NONE);
        CK(tui_screen_char(s, 1, 1) == '[', "button '[' at x");
        CK(tui_screen_char(s, 2, 1) == ' ', "button space");
        CK(tui_screen_char(s, 3, 1) == 'H', "button label H");
        CK(tui_screen_char(s, 6, 1) == ']', "button ']'");
        tui_screen_free(s);

        /* hit test: inside vs outside */
        CK(tui_hit(2, 1, 1, 1, 6, 1) == 1, "hit inside button");
        CK(tui_hit(8, 1, 1, 1, 6, 1) == 0, "hit outside button (x)");
        CK(tui_hit(2, 5, 1, 1, 6, 1) == 0, "hit outside button (y)");
    }

    /* ---------- tab bar ---------- */
    {
        TuiTab tabs[3];
        tabs[0].label = "notes.md"; tabs[0].active = 1;
        tabs[1].label = "a.txt";     tabs[1].active = 0;
        tabs[2].label = "longname.log"; tabs[2].active = 0;
        size_t end = tui_tabbar_layout(tabs, 3, 0);
        CK(end > 0, "tabbar layout returns end column");
        /* tab0 [ notes.md x ] -> 2 + 8 + 3 = 13 wide ("notes.md" is 8 chars) */
        CK(tabs[0].w == 13, "tab0 width = 13");
        CK(tabs[0].x == 0, "tab0 at x=0");
        CK(tabs[1].x == 14, "tab1 at x=14 (gap after tab0)");
        /* draw + click hit-test */
        TuiScreen *s = tui_screen_create(60, 3);
        tui_tabbar(s, 1, tabs, 3, 0);
        CK(tui_screen_char(s, 0, 1) == '[', "tab0 '[' at x0");
        CK(tui_screen_char(s, 1, 1) == ' ', "tab0 space");
        CK(tui_screen_char(s, 11, 1) == 'x', "tab0 close 'x'");
        CK(tui_screen_char(s, 12, 1) == ']', "tab0 ']'");
        /* active tab is reverse-video */
        CK(tui_screen_attr(s, 2, 1) == (TUI_ATTR_REVERSE | TUI_ATTR_BOLD),
            "active tab reverse+bold");
        tui_screen_free(s);
        /* hit-test: clicking tab1's label cell selects tab1 */
        CK(tui_tabbar_hit(tabs, 3, 16, 1, 1) == 1, "hit tab1 label -> 1");
        CK(tui_tabbar_hit(tabs, 3, 0, 1, 1) == 0, "hit tab0 -> 0");
        CK(tui_tabbar_hit(tabs, 3, 0, 0, 1) == -1, "hit wrong row -> -1");
        CK(tui_tabbar_hit(tabs, 3, 99, 1, 1) == -1, "hit past tabs -> -1");
    }

    if (fails) { printf("WUBUTUI TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUTUI TESTS PASSED\n");
    return 0;
}
