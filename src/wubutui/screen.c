/* screen.c -- off-screen cell grid + ANSI renderer. Pure C11, no TTY calls. */
#include "screen.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { char ch; uint8_t attr; } Cell;

struct TuiScreen {
    size_t w, h;
    Cell  *cell;   /* w*h, row-major */
};

TuiScreen *tui_screen_create(size_t w, size_t h) {
    if (w == 0 || h == 0 || w > SIZE_MAX / h) return NULL;
    TuiScreen *s = malloc(sizeof *s);
    if (!s) return NULL;
    s->w = w; s->h = h;
    s->cell = malloc(w * h * sizeof *s->cell);
    if (!s->cell) { free(s); return NULL; }
    for (size_t i = 0; i < w * h; i++) { s->cell[i].ch = ' '; s->cell[i].attr = TUI_ATTR_NONE; }
    return s;
}

void tui_screen_free(TuiScreen *s) {
    if (!s) return;
    free(s->cell);
    free(s);
}

size_t tui_screen_width(const TuiScreen *s)  { return s ? s->w : 0; }
size_t tui_screen_height(const TuiScreen *s) { return s ? s->h : 0; }

void tui_screen_clear(TuiScreen *s) {
    if (!s) return;
    for (size_t i = 0; i < s->w * s->h; i++) { s->cell[i].ch = ' '; s->cell[i].attr = TUI_ATTR_NONE; }
}

void tui_screen_put(TuiScreen *s, size_t x, size_t y, char ch, uint8_t attr) {
    if (!s || x >= s->w || y >= s->h) return;
    unsigned char uc = (unsigned char)ch;
    if (uc < 0x20 || uc == 0x7f) ch = ' ';   /* keep the grid printable */
    s->cell[y * s->w + x].ch = ch;
    s->cell[y * s->w + x].attr = attr;
}

char tui_screen_char(const TuiScreen *s, size_t x, size_t y) {
    if (!s || x >= s->w || y >= s->h) return ' ';
    return s->cell[y * s->w + x].ch;
}

uint8_t tui_screen_attr(const TuiScreen *s, size_t x, size_t y) {
    if (!s || x >= s->w || y >= s->h) return TUI_ATTR_NONE;
    return s->cell[y * s->w + x].attr;
}

char *tui_screen_dump(const TuiScreen *s) {
    if (!s) return NULL;
    size_t n = s->h * (s->w + 1) + 1;   /* each row + '\n', plus NUL */
    char *out = malloc(n);
    if (!out) return NULL;
    char *w = out;
    for (size_t y = 0; y < s->h; y++) {
        for (size_t x = 0; x < s->w; x++) *w++ = s->cell[y * s->w + x].ch;
        *w++ = '\n';
    }
    *w = '\0';
    return out;
}

void tui_screen_copy(TuiScreen *dst, const TuiScreen *src) {
    if (!dst || !src || dst->w != src->w || dst->h != src->h) return;
    memcpy(dst->cell, src->cell, dst->w * dst->h * sizeof *dst->cell);
}

/* --- ANSI rendering --- */

/* A small growable byte buffer for building the escape stream. */
typedef struct { char *p; size_t n, cap; int oom; } Buf;

static void buf_ensure(Buf *b, size_t extra) {
    if (b->oom) return;
    if (b->n + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->n + extra + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) { b->oom = 1; return; }
        b->p = np; b->cap = nc;
    }
}
static void buf_puts(Buf *b, const char *s) {
    size_t l = strlen(s);
    buf_ensure(b, l);
    if (b->oom) return;
    memcpy(b->p + b->n, s, l);
    b->n += l;
}
static void buf_putc(Buf *b, char c) {
    buf_ensure(b, 1);
    if (b->oom) return;
    b->p[b->n++] = c;
}

/* Emit the SGR sequence for `attr` (always a full reset + set for simplicity
 * and correctness across terminals). */
static void emit_sgr(Buf *b, uint8_t attr) {
    buf_puts(b, "\x1b[0");
    if (attr & TUI_ATTR_BOLD)      buf_puts(b, ";1");
    if (attr & TUI_ATTR_DIM)       buf_puts(b, ";2");
    if (attr & TUI_ATTR_UNDERLINE) buf_puts(b, ";4");
    if (attr & TUI_ATTR_REVERSE)   buf_puts(b, ";7");
    buf_putc(b, 'm');
}

/* Move cursor to 1-based (row,col). */
static void emit_move(Buf *b, size_t row1, size_t col1) {
    char tmp[64];
    snprintf(tmp, sizeof tmp, "\x1b[%zu;%zuH", row1, col1);
    buf_puts(b, tmp);
}

char *tui_screen_render(const TuiScreen *s, const TuiScreen *prev, size_t *out_len) {
    if (!s) return NULL;
    int diff = (prev && prev->w == s->w && prev->h == s->h);
    Buf b = {0};

    buf_puts(&b, "\x1b[H");           /* home */
    uint8_t cur_attr = TUI_ATTR_NONE;
    emit_sgr(&b, cur_attr);

    for (size_t y = 0; y < s->h; y++) {
        int line_started = 0;
        for (size_t x = 0; x < s->w; x++) {
            Cell c = s->cell[y * s->w + x];
            if (diff) {
                Cell pc = prev->cell[y * s->w + x];
                if (pc.ch == c.ch && pc.attr == c.attr) { line_started = 0; continue; }
                /* moved past a skipped cell -> reposition */
                if (!line_started) emit_move(&b, y + 1, x + 1);
            } else if (x == 0) {
                emit_move(&b, y + 1, 1);
            }
            if (c.attr != cur_attr) { emit_sgr(&b, c.attr); cur_attr = c.attr; }
            buf_putc(&b, c.ch);
            line_started = 1;
        }
    }
    emit_sgr(&b, TUI_ATTR_NONE);      /* leave clean */

    if (b.oom) { free(b.p); return NULL; }
    if (!b.p) { b.p = malloc(1); if (!b.p) return NULL; b.p[0] = '\0'; }
    else b.p[b.n] = '\0';
    if (out_len) *out_len = b.n;
    return b.p;
}
