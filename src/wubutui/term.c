/* term.c -- POSIX raw-mode terminal edge for wubutui. */
#define _POSIX_C_SOURCE 200809L
#include "term.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

struct TuiTerm {
    struct termios saved;
    int in_fd, out_fd;
    TuiScreen *prev;   /* last painted frame (for diffing) */
};

TuiTerm *tui_term_enter(void) {
    if (!isatty(STDOUT_FILENO)) return NULL;
    TuiTerm *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->in_fd = STDIN_FILENO;
    t->out_fd = STDOUT_FILENO;

    if (tcgetattr(t->in_fd, &t->saved) != 0) { free(t); return NULL; }
    struct termios raw = t->saved;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;    /* block for at least one byte */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(t->in_fd, TCSAFLUSH, &raw) != 0) { free(t); return NULL; }

    /* alt screen, clear, hide cursor, then enable mouse reporting:
     *   1000 = button press/release, 1002 = button-drag motion,
     *   1006 = SGR extended coords (works past column 223, unambiguous).
     * Sending both 1000 and 1002 plus 1006 gives us clicks, drags and wheel. */
    const char *init = "\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l"
                       "\x1b[?1000h\x1b[?1002h\x1b[?1006h";
    ssize_t wr = write(t->out_fd, init, strlen(init));
    (void)wr;
    return t;
}

void tui_term_leave(TuiTerm *t) {
    if (!t) return;
    /* disable mouse reporting, show cursor, leave alt screen, reset SGR */
    const char *fin = "\x1b[?1006l\x1b[?1002l\x1b[?1000l"
                      "\x1b[?25h\x1b[0m\x1b[?1049l";
    ssize_t wr = write(t->out_fd, fin, strlen(fin));
    (void)wr;
    tcsetattr(t->in_fd, TCSAFLUSH, &t->saved);
    tui_screen_free(t->prev);
    free(t);
}

void tui_term_size(TuiTerm *t, size_t *w, size_t *h) {
    size_t cw = 80, ch = 24;
    struct winsize ws;
    if (t && ioctl(t->out_fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        cw = ws.ws_col; ch = ws.ws_row;
    }
    if (w) *w = cw;
    if (h) *h = ch;
}

void tui_term_present(TuiTerm *t, const TuiScreen *s) {
    if (!t || !s) return;
    /* if size changed, drop the stale prev frame so we do a full paint */
    if (t->prev && (tui_screen_width(t->prev) != tui_screen_width(s) ||
                    tui_screen_height(t->prev) != tui_screen_height(s))) {
        tui_screen_free(t->prev);
        t->prev = NULL;
    }
    size_t len = 0;
    char *bytes = tui_screen_render(s, t->prev, &len);
    if (bytes) {
        ssize_t wr = write(t->out_fd, bytes, len);
        (void)wr;
        free(bytes);
    }
    /* snapshot current frame as prev */
    if (!t->prev) t->prev = tui_screen_create(tui_screen_width(s), tui_screen_height(s));
    if (t->prev) tui_screen_copy(t->prev, s);
}

size_t tui_term_read(TuiTerm *t, char *buf, size_t cap) {
    if (!t || !buf || cap == 0) return 0;
    ssize_t n = read(t->in_fd, buf, cap);
    if (n <= 0) return 0;
    return (size_t)n;
}
