/* wubuview -- native terminal document viewer for the WuBuOffice suite.
 *
 * The human-facing GUI: open ANY format wubudoc supports (docx/odt/md/html/
 * csv/xlsx/...), flatten its normalized model to text, and read it in a
 * scrollable, word-wrapped, paginated terminal window. Pure C11 + POSIX; no
 * Electron, no toolkit, no dependency -- consistent with the whole suite.
 *
 * Keys:  j / Down  scroll down     k / Up   scroll up
 *        Space / PgDn page down     b / PgUp page up
 *        g / Home    top            G / End  bottom
 *        q / Esc     quit
 *
 * A non-interactive mode (--dump) prints the flattened text and exits, so the
 * pipeline is scriptable and testable without a TTY.
 */
#define _POSIX_C_SOURCE 200809L
#include "docflat.h"
#include "wubudoc.h"
#include "screen.h"
#include "draw.h"
#include "input.h"
#include "term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *load_text_for(const char *path) {
    DocSession *s = doc_session_create();
    if (!s) return NULL;
    long id = doc_open(s, path);
    char *text = NULL;
    if (id >= 0) {
        char *model = doc_json(s, id);
        if (model) { text = docflat_from_json(model); free(model); }
    }
    doc_session_free(s);
    return text;
}

/* count wrapped lines for the current width (for scroll bounds) */
static size_t count_lines(const char *text, size_t width) {
    size_t n = 0;
    char **lines = tui_wrap(text, width, &n);
    tui_wrap_free(lines, n);
    return n;
}

static void draw_frame(TuiScreen *sc, const char *title, const char *text,
                       size_t scroll, size_t total) {
    size_t W = tui_screen_width(sc), H = tui_screen_height(sc);
    tui_screen_clear(sc);

    /* header bar */
    tui_hline(sc, 0, 0, W, ' ', TUI_ATTR_REVERSE);
    char hdr[256];
    snprintf(hdr, sizeof hdr, " wubuview  %.*s", (int)(W > 12 ? W - 12 : 0), title);
    tui_text(sc, 0, 0, hdr, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);

    /* body: word-wrapped text between header and footer */
    size_t body_y = 1, body_h = (H > 2) ? H - 2 : 0;
    tui_text_wrapped(sc, 1, body_y, (W > 2 ? W - 2 : 1), body_h, text, scroll, TUI_ATTR_NONE);

    /* footer bar with position */
    size_t fy = H - 1;
    tui_hline(sc, 0, fy, W, ' ', TUI_ATTR_REVERSE);
    size_t shown_end = scroll + body_h;
    if (shown_end > total) shown_end = total;
    int pct = total ? (int)((shown_end * 100) / total) : 100;
    char ftr[128];
    snprintf(ftr, sizeof ftr, " %zu-%zu/%zu (%d%%)  [j/k scroll  Space/b page  g/G ends  q quit]",
             total ? scroll + 1 : 0, shown_end, total, pct);
    tui_text(sc, 0, fy, ftr, TUI_ATTR_REVERSE);
}

static int run_interactive(const char *title, const char *text) {
    TuiTerm *t = tui_term_enter();
    if (!t) {
        fprintf(stderr, "wubuview: not a terminal (use --dump for piping)\n");
        return 1;
    }
    size_t scroll = 0;
    char inbuf[64];
    int running = 1;
    size_t prev_w = 0, prev_h = 0, total = 0;

    while (running) {
        size_t W, H;
        tui_term_size(t, &W, &H);
        if (W < 4) W = 4;
        if (H < 3) H = 3;
        size_t body_h = H - 2;
        if (W != prev_w || H != prev_h) {
            total = count_lines(text, W > 2 ? W - 2 : 1);
            prev_w = W; prev_h = H;
        }
        /* clamp scroll */
        size_t max_scroll = (total > body_h) ? total - body_h : 0;
        if (scroll > max_scroll) scroll = max_scroll;

        TuiScreen *sc = tui_screen_create(W, H);
        if (!sc) break;
        draw_frame(sc, title, text, scroll, total);
        tui_term_present(t, sc);
        tui_screen_free(sc);

        size_t got = tui_term_read(t, inbuf, sizeof inbuf);
        if (got == 0) break;
        size_t off = 0;
        while (off < got) {
            TuiKey k;
            size_t used = tui_key_decode(inbuf + off, got - off, &k);
            if (used == 0) break;   /* incomplete: drop remainder, read again */
            off += used;
            switch (k.type) {
                case TUI_KEY_CHAR:
                    if (k.ch == 'q') running = 0;
                    else if (k.ch == 'j') { if (scroll < max_scroll) scroll++; }
                    else if (k.ch == 'k') { if (scroll) scroll--; }
                    else if (k.ch == ' ') scroll = (scroll + body_h < max_scroll) ? scroll + body_h : max_scroll;
                    else if (k.ch == 'b') scroll = (scroll > body_h) ? scroll - body_h : 0;
                    else if (k.ch == 'g') scroll = 0;
                    else if (k.ch == 'G') scroll = max_scroll;
                    break;
                case TUI_KEY_ESC:       running = 0; break;
                case TUI_KEY_DOWN:      if (scroll < max_scroll) scroll++; break;
                case TUI_KEY_UP:        if (scroll) scroll--; break;
                case TUI_KEY_PAGE_DOWN: scroll = (scroll + body_h < max_scroll) ? scroll + body_h : max_scroll; break;
                case TUI_KEY_PAGE_UP:   scroll = (scroll > body_h) ? scroll - body_h : 0; break;
                case TUI_KEY_HOME:      scroll = 0; break;
                case TUI_KEY_END:       scroll = max_scroll; break;
                default: break;
            }
        }
    }
    tui_term_leave(t);
    return 0;
}

int wubuview_main(int argc, char **argv) {
    const char *path = NULL;
    int dump = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump") == 0) dump = 1;
        else if (argv[i][0] != '-') path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: %s [--dump] <document>\n"
                        "  opens any wubudoc-supported format in a scrollable viewer\n", argv[0]);
        return 2;
    }

    char *text = load_text_for(path);
    if (!text) { fprintf(stderr, "wubuview: cannot open %s\n", path); return 1; }
    if (!text[0]) { free(text); text = NULL;
        text = malloc(32); if (text) strcpy(text, "(document has no text content)"); }

    int rc;
    if (dump) { fputs(text, stdout); if (text[0] && text[strlen(text)-1] != '\n') fputc('\n', stdout); rc = 0; }
    else rc = run_interactive(path, text);

    free(text);
    return rc;
}
