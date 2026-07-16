/* wubuview -- native terminal document viewer with multi-tab support.
 *
 * Open ANY format wubudoc supports (docx/odt/md/html/csv/xlsx/...),
 * flatten to text, and read it in a scrollable, word-wrapped, paginated
 * terminal window. Multiple documents open as clickable tabs (Notepad++-style).
 * Pure C11 + POSIX; no Electron, no toolkit, no dependency.
 *
 * Keys:  j/Down scroll   k/Up scroll up     Space/PgDn page down
 *        b/PgUp page up   g/Home top         G/End bottom
 *        Ctrl-I next tab   Ctrl-U prev tab       q/Esc quit
 * Mouse: wheel scrolls; click/drag the right-edge scrollbar to jump;
 *        click a tab to switch; click footer [Top][PgUp][PgDn][Bot][Quit].
 * A non-interactive mode (--dump) prints the flattened text and exits.
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

#define MAX_PATHS 8

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

/* footer button labels (kept in sync with controller.c's VB_LABELS) */
static const char *BTN_LABELS[VB__COUNT] = {
    [VB_TOP] = "Top", [VB_PGUP] = "PgUp", [VB_PGDN] = "PgDn",
    [VB_BOT] = "Bot", [VB_QUIT] = "Quit",
};

static void draw_frame(TuiScreen *sc, const char **texts, const VState *st) {
    size_t W = tui_screen_width(sc), H = tui_screen_height(sc);
    tui_screen_clear(sc);

    /* header bar */
    tui_hline(sc, 0, 0, W, ' ', TUI_ATTR_REVERSE);
    char hdr[256];
    snprintf(hdr, sizeof hdr, " wubuview  %zu tab%s", st->tab_n, st->tab_n == 1 ? "" : "s");
    tui_text(sc, 0, 0, hdr, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);

    /* tab bar (row 1) -- clickable */
    TuiTab tabs[VCTRL_MAX_TABS];
    for (size_t i = 0; i < st->tab_n; i++) {
        tabs[i].label = st->tabs[i].title;
        tabs[i].active = (i == st->tab_active);
        tabs[i].x = tabs[i].w = 0;
    }
    if (st->tab_n) tui_tabbar(sc, 1, tabs, st->tab_n, 1);

    /* body: word-wrapped text between tab bar and footer */
    size_t body_w = (W > 3) ? W - 3 : 1;
    const char *txt = texts[st->tab_active];
    tui_text_wrapped(sc, 1, st->body_y, body_w, st->body_h,
                      txt ? txt : "", st->tabs[st->tab_active].scroll, TUI_ATTR_NONE);

    /* scrollbar on the right edge */
    if (W >= 2 && st->body_h > 0)
        tui_scrollbar(sc, W - 1, st->body_y, st->body_h,
                     st->tabs[st->tab_active].total, st->body_h,
                     st->tabs[st->tab_active].scroll);

    /* footer: position readout + buttons */
    size_t fy = H - 1;
    tui_hline(sc, 0, fy, W, ' ', TUI_ATTR_REVERSE);
    size_t sc2 = st->tabs[st->tab_active].scroll;
    size_t tot = st->tabs[st->tab_active].total;
    size_t shown_end = sc2 + st->body_h;
    if (shown_end > tot) shown_end = tot;
    int pct = tot ? (int)((shown_end * 100) / tot) : 100;
    char pos[64];
    snprintf(pos, sizeof pos, " %zu-%zu/%zu (%d%%)  ",
             tot ? sc2 + 1 : 0, shown_end, tot, pct);
    tui_text(sc, 0, fy, pos, TUI_ATTR_REVERSE);

    for (int i = 0; i < (int)VB__COUNT; i++) {
        size_t x = st->btn_x[i];
        if (x + st->btn_w[i] <= W)
            tui_button(sc, x, fy, BTN_LABELS[i], TUI_ATTR_REVERSE | TUI_ATTR_BOLD);
    }

    /* command palette (Ctrl+K): render a prompt bar over the footer */
    if (st->palette) {
        tui_hline(sc, 0, fy, W, ' ', TUI_ATTR_REVERSE | TUI_ATTR_BOLD);
        char p[320];
        snprintf(p, sizeof p, " :%s", st->pal_buf);
        tui_text(sc, 0, fy, p, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);
        /* show the matching command hint */
        char hint[64];
        const char *match = NULL;
        for (int i = 0; i < VCMD_N; i++)
            if (strncmp(VCMD[i], st->pal_buf, st->pal_len) == 0) { match = VCMD[i]; break; }
        if (match && (st->btn_x[0] + 30) < W)
            snprintf(hint, sizeof hint, "  > %s", match);
        else if (!st->pal_len)
            snprintf(hint, sizeof hint, "  top bottom pgup pgdn quit nexttab prevtab");
        else
            snprintf(hint, sizeof hint, "  (no match)");
        size_t hx = strlen(p) + 1;
        if (hx < W) tui_text(sc, hx, fy, hint, TUI_ATTR_REVERSE);
    }
}

static int run_interactive(const char **paths, size_t npaths) {
    TuiTerm *t = tui_term_enter();
    if (!t) {
        fprintf(stderr, "wubuview: not a terminal (use --dump for piping)\n");
        return 1;
    }
    size_t prev_w = 0, prev_h = 0;
    VState st;
    memset(&st, 0, sizeof st);
    tui_key_state_init(&st.keyst);

    char *texts[VCTRL_MAX_TABS];
    memset(texts, 0, sizeof texts);
    DocSession *drop_sess = doc_session_create();

    /* open every requested document as a tab */
    for (size_t i = 0; i < npaths && i < VCTRL_MAX_TABS; i++) {
        if (vctrl_open(&st, paths[i], 0) < 0) {
            fprintf(stderr, "wubuview: tab limit (%d) reached\n", VCTRL_MAX_TABS);
            break;
        }
        texts[i] = load_text_for(paths[i]);
    }

    char inbuf[256];
    while (st.running) {
        size_t W, H;
        tui_term_size(t, &W, &H);
        if (W < 4) W = 4;
        if (H < 4) H = 4;
        size_t body_w = (W > 3) ? W - 3 : 1;
        if (W != prev_w || H != prev_h) {
            for (size_t i = 0; i < st.tab_n; i++) {
                size_t tot = count_lines(texts[i], body_w);
                if (st.tab_active == i && st.screen_w == 0)
                    vctrl_init(&st, W, H);
                else if (st.tab_active == i)
                    vctrl_resize(&st, W, H, tot);
                else
                    st.tabs[i].total = tot;  /* keep other tabs' bounds fresh */
            }
            prev_w = W; prev_h = H;
        }

        TuiScreen *sc = tui_screen_create(W, H);
        if (!sc) break;
        draw_frame(sc, (const char **)texts, &st);
        tui_term_present(t, sc);
        tui_screen_free(sc);

        size_t got = tui_term_read(t, inbuf, sizeof inbuf);
        if (got == 0) break;
        size_t off = 0;
        while (off < got) {
            TuiKey k;
            size_t used = tui_key_decode_s(inbuf + off, got - off, &k, &st.keyst);
            if (used == 0) break;
            off += used;
            if (k.type == TUI_KEY_PASTE) {
                /* A drag/drop or bracketed paste: interpret the bytes as a
                 * document (file path, or the file contents) and open the
                 * extracted text in a new tab. */
                char *txt = doc_drop_text(drop_sess, (const uint8_t *)k.paste_data, k.paste_len);
                if (txt) {
                    size_t idx = vctrl_open(&st, "[dropped]", 0);
                    if (idx != (size_t)-1 && idx < VCTRL_MAX_TABS) {
                        texts[idx] = txt;
                        size_t tot = count_lines(txt, body_w);
                        vctrl_resize(&st, W, H, tot);
                    } else {
                        free(txt);
                    }
                }
                continue;
            }
            VBtn b = vctrl_handle(&st, &k);
            /* a tab click on the tabbar switches the active doc */
            if (k.type == TUI_KEY_MOUSE && k.mouse_action == TUI_MOUSE_PRESS
                && k.mouse_y == 1) {
                TuiTab ht[VCTRL_MAX_TABS];
                for (size_t i = 0; i < st.tab_n; i++) {
                    ht[i].label = st.tabs[i].title;
                    ht[i].active = (i == st.tab_active);
                    ht[i].x = ht[i].w = 0;
                }
                int hit = tui_tabbar_hit(ht, st.tab_n, k.mouse_x, 1, 1);
                if (hit >= 0) st.tab_active = (size_t)hit;
            }
            (void)b;
        }
    }
    tui_term_leave(t);
    for (size_t i = 0; i < VCTRL_MAX_TABS; i++) free(texts[i]);
    doc_session_free(drop_sess);
    tui_key_state_free(&st.keyst);
    return 0;
}

int wubuview_main(int argc, char **argv) {
    const char *paths[MAX_PATHS];
    size_t n = 0;
    int dump = 0;
    for (int i = 1; i < argc && n < MAX_PATHS; i++) {
        if (strcmp(argv[i], "--dump") == 0) dump = 1;
        else if (argv[i][0] != '-') paths[n++] = argv[i];
    }
    if (n == 0) {
        fprintf(stderr,
            "usage: %s [--dump] <doc1> [<doc2> ...]\n"
            "  opens any wubudoc-supported format in a scrollable, tabbed viewer\n",
            argv[0]);
        return 2;
    }

    char *texts[MAX_PATHS];
    for (size_t i = 0; i < n; i++) {
        texts[i] = load_text_for(paths[i]);
        if (!texts[i]) { fprintf(stderr, "wubuview: cannot open %s\n", paths[i]); }
        else if (!texts[i][0]) { free(texts[i]); texts[i] = strdup("(document has no text content)"); }
    }

    int rc;
    if (dump) {
        for (size_t i = 0; i < n; i++) {
            if (n > 1) printf("===== %s =====\n", paths[i]);
            fputs(texts[i], stdout);
            if (texts[i][0] && texts[i][strlen(texts[i]) - 1] != '\n') fputc('\n', stdout);
        }
        rc = 0;
    } else {
        rc = run_interactive(paths, n);
    }

    for (size_t i = 0; i < n; i++) free(texts[i]);
    return rc;
}
