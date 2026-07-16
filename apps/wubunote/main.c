/* wubunote -- native tabbed terminal text editor (Notepad++-class).
 *
 * Open any UTF-8/text file, edit it in a scrollable, optionally word-wrapped
 * window with line numbers, tabs, a command palette (Ctrl+K) and a status
 * bar. Pure C11 + POSIX; no toolkit, no dependency.
 *
 * Keys (normal):  arrows/home/end/ins/del   type/enter/backspace
 *        Ctrl+S  save         Ctrl+W  toggle word-wrap
 *        Ctrl+F  find         Ctrl+G  goto line
 *        Ctrl+K  command palette   Ctrl+B/Ctrl+E  prev/next tab
 *        Ctrl+Q / Esc  quit
 * Mouse: click a tab to switch. In a prompt, type + Enter to commit,
 *        Esc to cancel.
 */
#define _POSIX_C_SOURCE 200809L
#include "note_controller.h"
#include "screen.h"
#include "draw.h"
#include "input.h"
#include "term.h"
#include "wubudoc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* flattened text of the active buffer (caller frees) */
static char *active_text(NState *st) {
    return edit_serialize(st->tabs[st->tab_active].buf);
}

static void draw(NState *st, TuiScreen *sc) {
    size_t W = tui_screen_width(sc), H = tui_screen_height(sc);
    tui_screen_clear(sc);

    /* header */
    tui_hline(sc, 0, 0, W, ' ', TUI_ATTR_REVERSE);
    char hdr[256];
    snprintf(hdr, sizeof hdr, " wubunote  %zu tab%s  [Ctrl+K help]",
             st->tab_n, st->tab_n == 1 ? "" : "s");
    tui_text(sc, 0, 0, hdr, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);

    /* tab bar */
    TuiTab tabs[NCTRL_MAX_TABS];
    for (size_t i = 0; i < st->tab_n; i++) {
        tabs[i].label = st->tabs[i].path;
        tabs[i].active = (i == st->tab_active);
        tabs[i].x = tabs[i].w = 0;
    }
    if (st->tab_n) tui_tabbar(sc, 1, tabs, st->tab_n, 1);

    /* body: line numbers (gutter) + wrapped text */
    char *text = active_text(st);
    size_t gut = st->gutter;
    size_t body_w = (W > gut + 1) ? W - gut - 1 : 1;
    size_t total = 0;
    char **lines = tui_wrap(text ? text : "", body_w, &total);
    EditBuf *b = st->tabs[st->tab_active].buf;
    size_t cur = b ? edit_cursor_row(b) : 0;
    (void)cur;
    for (size_t r = 0; r < st->body_h && r < total; r++) {
        size_t y = st->body_y + r;
        if (gut) {
            char num[16];
            snprintf(num, sizeof num, "%*zu ", (int)(gut - 1), r + 1);
            tui_text(sc, 0, y, num, TUI_ATTR_DIM);
        }
        const char *row = (lines && r < total) ? lines[r] : "";
        tui_text(sc, gut, y, row, TUI_ATTR_NONE);
    }
    tui_wrap_free(lines, total);
    free(text);

    /* status / prompt bar */
    size_t fy = H - 1;
    tui_hline(sc, 0, fy, W, ' ', TUI_ATTR_REVERSE);
    if (st->prompt != NPMT_NONE) {
        char p[320];
        snprintf(p, sizeof p, " %s %s", st->prompt_label, st->prompt_buf);
        tui_text(sc, 0, fy, p, TUI_ATTR_REVERSE | TUI_ATTR_BOLD);
        if (st->prompt == NPMT_CMD) {
            /* show the command palette list + selection */
            char pal[256];
            size_t off = strlen(p) + 1;
            snprintf(pal, sizeof pal, "  [%s] %s", NCMD_HELP[st->cmd_sel], NCMD[st->cmd_sel]);
            if (off < W) tui_text(sc, off, fy, pal, TUI_ATTR_REVERSE);
        }
    } else {
        EditBuf *b = st->tabs[st->tab_active].buf;
        char s[256];
        snprintf(s, sizeof s, " Ln %zu/%zu  Col %zu  %s  %s",
                 b ? edit_cursor_row(b) + 1 : 0,
                 b ? edit_line_count(b) : 0,
                 b ? edit_cursor_col(b) + 1 : 0,
                 st->wrap ? "Wrap" : "NoWrap",
                 st->tabs[st->tab_active].dirty ? "*" : " ");
        tui_text(sc, 0, fy, s, TUI_ATTR_REVERSE);
    }
}

static int run_interactive(int npaths, char **paths) {
    TuiTerm *t = tui_term_enter();
    if (!t) { fprintf(stderr, "wubunote: not a terminal\n"); return 1; }
    NState st;
    memset(&st, 0, sizeof st);
    tui_key_state_init(&st.keyst);
    DocSession *drop_sess = doc_session_create();
    size_t prev_w = 0, prev_h = 0;
    if (npaths == 0) nctrl_new(&st);
    else for (int i = 0; i < npaths; i++) nctrl_open(&st, paths[i]);

    char inbuf[256];
    while (st.running) {
        size_t W, H;
        tui_term_size(t, &W, &H);
        if (W < 6) W = 6;
        if (H < 5) H = 5;
        if (W != prev_w || H != prev_h) {
            if (st.screen_w == 0) nctrl_init(&st, W, H);
            else nctrl_resize(&st, W, H);
            prev_w = W; prev_h = H;
        }

        TuiScreen *sc = tui_screen_create(W, H);
        if (!sc) break;
        draw(&st, sc);
        tui_term_present(t, sc);
        tui_screen_free(sc);

        char *save_path = NULL;
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
                 * document and insert the extracted text into the buffer. */
                char *txt = doc_drop_text(drop_sess,
                                          (const uint8_t *)k.paste_data, k.paste_len);
                EditBuf *b = nctrl_active_buf(&st);
                if (txt && b) {
                    for (char *p = txt; *p; p++) {
                        if (*p == '\n') edit_new_line(b);
                        else edit_put_char(b, *p);
                    }
                }
                free(txt);
                continue;
            }
            nctrl_handle(&st, &k, &save_path);
            if (save_path) {
                char *s = active_text(&st);
                if (s) {
                    FILE *f = fopen(save_path, "wb");
                    if (f) { fwrite(s, 1, strlen(s), f); fclose(f); }
                    free(s);
                }
                free(save_path); save_path = NULL;
            }
        }
        (void)save_path;
    }
    tui_term_leave(t);

    /* reminder saves on quit if dirty */
    for (size_t i = 0; i < st.tab_n; i++) edit_free(st.tabs[i].buf);
    doc_session_free(drop_sess);
    tui_key_state_free(&st.keyst);
    return 0;
}

int wubunote_main(int argc, char **argv) {
    char *paths[NCTRL_MAX_TABS];
    int n = 0;
    for (int i = 1; i < argc && n < NCTRL_MAX_TABS; i++)
        if (argv[i][0] != '-') paths[n++] = argv[i];
    return run_interactive(n, paths);
}
