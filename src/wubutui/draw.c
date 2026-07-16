/* draw.c -- drawing primitives + pure word wrap. */
#include "draw.h"

#include <stdlib.h>
#include <string.h>

void tui_text(TuiScreen *s, size_t x, size_t y, const char *str, uint8_t attr) {
    if (!s || !str) return;
    for (size_t i = 0; str[i]; i++) tui_screen_put(s, x + i, y, str[i], attr);
}

void tui_hline(TuiScreen *s, size_t x, size_t y, size_t n, char ch, uint8_t attr) {
    for (size_t i = 0; i < n; i++) tui_screen_put(s, x + i, y, ch, attr);
}

void tui_vline(TuiScreen *s, size_t x, size_t y, size_t n, char ch, uint8_t attr) {
    for (size_t i = 0; i < n; i++) tui_screen_put(s, x, y + i, ch, attr);
}

void tui_fill(TuiScreen *s, size_t x, size_t y, size_t w, size_t h, char ch, uint8_t attr) {
    for (size_t j = 0; j < h; j++)
        for (size_t i = 0; i < w; i++)
            tui_screen_put(s, x + i, y + j, ch, attr);
}

void tui_box(TuiScreen *s, size_t x, size_t y, size_t w, size_t h, uint8_t attr) {
    if (!s || w < 2 || h < 2) return;
    tui_hline(s, x, y, w, '-', attr);
    tui_hline(s, x, y + h - 1, w, '-', attr);
    tui_vline(s, x, y, h, '|', attr);
    tui_vline(s, x + w - 1, y, h, '|', attr);
    tui_screen_put(s, x, y, '+', attr);
    tui_screen_put(s, x + w - 1, y, '+', attr);
    tui_screen_put(s, x, y + h - 1, '+', attr);
    tui_screen_put(s, x + w - 1, y + h - 1, '+', attr);
}

/* --- word wrap --- */

/* Growable array of line pointers. */
typedef struct { char **v; size_t n, cap; int oom; } LineVec;

static void lv_push(LineVec *lv, const char *start, size_t len) {
    if (lv->oom) return;
    if (lv->n + 1 > lv->cap) {
        size_t nc = lv->cap ? lv->cap * 2 : 8;
        char **nv = realloc(lv->v, nc * sizeof *nv);
        if (!nv) { lv->oom = 1; return; }
        lv->v = nv; lv->cap = nc;
    }
    char *line = malloc(len + 1);
    if (!line) { lv->oom = 1; return; }
    if (len) memcpy(line, start, len);
    line[len] = '\0';
    lv->v[lv->n++] = line;
}

/* Wrap one hard line (no embedded '\n') into lv. */
static void wrap_hard_line(LineVec *lv, const char *s, size_t len, size_t width) {
    if (len == 0) { lv_push(lv, "", 0); return; }
    size_t i = 0;
    while (i < len) {
        /* skip leading spaces at the start of a wrapped segment */
        while (i < len && s[i] == ' ') i++;
        if (i >= len) break;
        size_t seg_start = i;
        size_t last_space = (size_t)-1;
        size_t j = i;
        while (j < len && (j - seg_start) < width) {
            if (s[j] == ' ') last_space = j;
            j++;
        }
        if (j >= len) {                       /* remainder fits */
            lv_push(lv, s + seg_start, len - seg_start);
            break;
        }
        if (s[j] == ' ') {                     /* break exactly at boundary space */
            lv_push(lv, s + seg_start, j - seg_start);
            i = j + 1;
        } else if (last_space != (size_t)-1) { /* break at last space in window */
            lv_push(lv, s + seg_start, last_space - seg_start);
            i = last_space + 1;
        } else {                               /* long word: hard break */
            lv_push(lv, s + seg_start, width);
            i = seg_start + width;
        }
    }
}

char **tui_wrap(const char *text, size_t width, size_t *out_count) {
    if (!text || width == 0) { if (out_count) *out_count = 0; return NULL; }
    LineVec lv = {0};
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t seg = nl ? (size_t)(nl - p) : strlen(p);
        wrap_hard_line(&lv, p, seg, width);
        if (!nl) break;
        p = nl + 1;
    }
    if (lv.oom) { tui_wrap_free(lv.v, lv.n); if (out_count) *out_count = 0; return NULL; }
    if (out_count) *out_count = lv.n;
    return lv.v;
}

void tui_wrap_free(char **lines, size_t count) {
    if (!lines) return;
    for (size_t i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

size_t tui_text_wrapped(TuiScreen *s, size_t x, size_t y, size_t w, size_t h,
                        const char *text, size_t scroll, uint8_t attr) {
    if (!s || w == 0 || h == 0 || !text) return 0;
    size_t n = 0;
    char **lines = tui_wrap(text, w, &n);
    if (!lines) return 0;
    for (size_t row = 0; row < h; row++) {
        size_t li = scroll + row;
        if (li >= n) break;
        tui_text(s, x, y + row, lines[li], attr);
    }
    tui_wrap_free(lines, n);
    return n;
}

/* --- scrollbar --- */

TuiThumb tui_scrollbar_thumb(size_t h, size_t total, size_t visible, size_t scroll) {
    TuiThumb t = { 0, h };
    if (h == 0) { t.len = 0; return t; }
    if (total <= visible || total == 0) { t.start = 0; t.len = h; return t; }

    /* thumb length proportional to visible/total, at least 1 */
    size_t len = (visible * h) / total;
    if (len == 0) len = 1;
    if (len > h) len = h;

    /* thumb start proportional to scroll / max_scroll over the free track */
    size_t max_scroll = total - visible;
    size_t free_track = h - len;
    size_t start = 0;
    if (max_scroll > 0 && free_track > 0) {
        if (scroll > max_scroll) scroll = max_scroll;
        start = (scroll * free_track) / max_scroll;
    }
    /* keep the thumb pinned to the ends at the extremes */
    if (scroll == 0) start = 0;
    else if (scroll >= max_scroll) start = free_track;
    if (start + len > h) start = h - len;

    t.start = start;
    t.len = len;
    return t;
}

void tui_scrollbar(TuiScreen *s, size_t x, size_t y, size_t h,
                   size_t total, size_t visible, size_t scroll) {
    if (!s || h == 0) return;
    TuiThumb th = tui_scrollbar_thumb(h, total, visible, scroll);
    for (size_t i = 0; i < h; i++) {
        int on_thumb = (i >= th.start && i < th.start + th.len);
        if (on_thumb)
            tui_screen_put(s, x, y + i, ' ', TUI_ATTR_REVERSE);
        else
            tui_screen_put(s, x, y + i, '|', TUI_ATTR_DIM);
    }
}

size_t tui_scrollbar_scroll_at(size_t h, size_t total, size_t visible, size_t row) {
    if (h == 0 || total <= visible) return 0;
    size_t max_scroll = total - visible;
    if (h <= 1) return 0;
    /* map the clicked row (0..h-1) linearly onto 0..max_scroll */
    size_t scroll = (row * max_scroll) / (h - 1);
    if (scroll > max_scroll) scroll = max_scroll;
    return scroll;
}

/* --- button --- */

size_t tui_button_width(const char *label) {
    return (label ? strlen(label) : 0) + 4;   /* "[ " + label + " ]" */
}

size_t tui_button(TuiScreen *s, size_t x, size_t y, const char *label, uint8_t attr) {
    if (!label) label = "";
    size_t w = tui_button_width(label);
    tui_screen_put(s, x, y, '[', attr);
    tui_screen_put(s, x + 1, y, ' ', attr);
    tui_text(s, x + 2, y, label, attr);
    size_t l = strlen(label);
    tui_screen_put(s, x + 2 + l, y, ' ', attr);
    tui_screen_put(s, x + 3 + l, y, ']', attr);
    return w;
}

int tui_hit(size_t px, size_t py, size_t x, size_t y, size_t w, size_t h) {
    return (px >= x && px < x + w && py >= y && py < y + h) ? 1 : 0;
}

/* --- tab bar widget --- */

size_t tui_tabbar_layout(TuiTab *tabs, size_t n, size_t x0) {
    if (!tabs || n == 0) return x0;
    size_t x = x0;
    for (size_t i = 0; i < n; i++) {
        const char *l = tabs[i].label ? tabs[i].label : "";
        /* "[ " + label + " x ]" -> len = 2 + label + 3 */
        size_t w = 2 + strlen(l) + 3;
        tabs[i].x = x;
        tabs[i].w = w;
        x += w + 1;   /* one-space gap between tabs */
    }
    return x;
}

size_t tui_tabbar(TuiScreen *s, size_t y, TuiTab *tabs, size_t n, size_t x0) {
    size_t end = tui_tabbar_layout(tabs, n, x0);
    if (!s) return end;
    for (size_t i = 0; i < n; i++) {
        const char *l = tabs[i].label ? tabs[i].label : "";
        size_t x = tabs[i].x;
        uint8_t attr = tabs[i].active ? (TUI_ATTR_REVERSE | TUI_ATTR_BOLD)
                                        : TUI_ATTR_DIM;
        tui_screen_put(s, x,     y, '[', attr);
        tui_screen_put(s, x + 1, y, ' ', attr);
        tui_text(s, x + 2, y, l, attr);
        size_t ll = strlen(l);
        tui_screen_put(s, x + 2 + ll, y, ' ', attr);
        tui_screen_put(s, x + 3 + ll, y, 'x', attr);   /* close affordance */
        tui_screen_put(s, x + 4 + ll, y, ']', attr);
    }
    return end;
}

int tui_tabbar_hit(TuiTab *tabs, size_t n, size_t px, size_t py, size_t bar_y) {
    if (py != bar_y || !tabs) return -1;
    for (size_t i = 0; i < n; i++) {
        if (px >= tabs[i].x && px < tabs[i].x + tabs[i].w) return (int)i;
    }
    return -1;
}
