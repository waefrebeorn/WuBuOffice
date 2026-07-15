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
 * Mouse: wheel scrolls; click/drag the scrollbar on the right edge to jump;
 *        click the footer buttons [Top] [PgUp] [PgDn] [Bot] [Quit].
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
#include "controller.h"

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

/* Footer button labels -- kept in sync with controller.c's VB_LABELS. */
static const char *BTN_LABELS[VB__COUNT] = {
    [VB_TOP]  = "Top", [VB_PGUP] = "PgUp", [VB_PGDN] = "PgDn",
    [VB_BOT]  = "Bot", [VB_QUIT] = "Quit",
};

static void draw_frame(TuiScreen *sc, const char *title, const char *text,
                       const VState *st) {
    size_t W = tui_screen_width(sc), H = tui_screen_height(sc);
    tui_screen_clear(sc);

    /* header bar */
    tui_hline(sc, 0, 0, W, ' ', TUI_ATTR_REVERSE);
    char hdr[256];
    snprintf(hdr, sizeof hdr, " wubuview  %.*s", (int)(W > 12 ? W - 12 : 0), title);
    tui_text(sc, 0, 0, hdr, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);

    /* body: word-wrapped text between header and footer, leaving 1 col for the
     * scrollbar on the right edge */
    size_t body_w = (W > 3) ? W - 3 : 1;   /* gutter, scrollbar, margin */
    tui_text_wrapped(sc, 1, st->body_y, body_w, st->body_h, text, st->scroll, TUI_ATTR_NONE);

    /* scrollbar on the right edge of the body */
    if (W >= 2 && st->body_h > 0)
        tui_scrollbar(sc, W - 1, st->body_y, st->body_h, st->total, st->body_h, st->scroll);

    /* footer bar: position readout + clickable buttons */
    size_t fy = H - 1;
    tui_hline(sc, 0, fy, W, ' ', TUI_ATTR_REVERSE);
    size_t shown_end = st->scroll + st->body_h;
    if (shown_end > st->total) shown_end = st->total;
    int pct = st->total ? (int)((shown_end * 100) / st->total) : 100;
    char pos[64];
    snprintf(pos, sizeof pos, " %zu-%zu/%zu (%d%%)  ",
             st->total ? st->scroll + 1 : 0, shown_end, st->total, pct);
    tui_text(sc, 0, fy, pos, TUI_ATTR_REVERSE);

    for (int i = 0; i < (int)VB__COUNT; i++) {
        size_t x = st->btn_x[i];
        if (x + st->btn_w[i] <= W)
            tui_button(sc, x, fy, BTN_LABELS[i], TUI_ATTR_REVERSE | TUI_ATTR_BOLD);
    }
}

static int run_interactive(const char *title, const char *text) {
    TuiTerm *t = tui_term_enter();
    if (!t) {
        fprintf(stderr, "wubuview: not a terminal (use --dump for piping)\n");
        return 1;
    }
    size_t prev_w = 0, prev_h = 0, total = 0;
    VState st;
    memset(&st, 0, sizeof st);

    char inbuf[256];
    while (st.running) {
        size_t W, H;
        tui_term_size(t, &W, &H);
        if (W < 4) W = 4;
        if (H < 3) H = 3;
        size_t body_w = (W > 3) ? W - 3 : 1;
        if (W != prev_w || H != prev_h) {
            total = count_lines(text, body_w);
            prev_w = W; prev_h = H;
            if (st.screen_w == 0) vctrl_init(&st, W, H, total);
            else                  vctrl_resize(&st, W, H, total);
        }

        TuiScreen *sc = tui_screen_create(W, H);
        if (!sc) break;
        draw_frame(sc, title, text, &st);
        tui_term_present(t, sc);
        tui_screen_free(sc);

        size_t got = tui_term_read(t, inbuf, sizeof inbuf);
        if (got == 0) break;
        size_t off = 0;
        while (off < got) {
            TuiKey k;
            size_t used = tui_key_decode(inbuf + off, got - off, &k);
            if (used == 0) break;   /* incomplete: read more */
            off += used;
            vctrl_handle(&st, &k);  /* ALL state transition lives in the controller */
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
