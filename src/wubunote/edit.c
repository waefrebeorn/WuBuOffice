/* edit.c -- wubunote text-buffer model (pure, allocation-checked). */
#include "edit.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct EditBuf {
    char **lines;
    size_t n, cap;
    size_t cur_row, cur_col;
    int    dirty;
};

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* replace line `row` with a copy of `s` */
static int set_line(EditBuf *b, size_t row, const char *s) {
    char *c = xstrdup(s);
    if (!c) return -1;
    free(b->lines[row]);
    b->lines[row] = c;
    return 0;
}

/* insert an empty line at `row`, shifting later lines down */
static int insert_line(EditBuf *b, size_t row) {
    if (b->n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 8;
        char **nl = realloc(b->lines, nc * sizeof *nl);
        if (!nl) return -1;
        b->lines = nl; b->cap = nc;
    }
    for (size_t i = b->n; i > row; i--) b->lines[i] = b->lines[i - 1];
    b->lines[row] = xstrdup("");
    if (!b->lines[row]) return -1;
    b->n++;
    return 0;
}

/* remove line `row`, shifting later lines up */
static void remove_line(EditBuf *b, size_t row) {
    free(b->lines[row]);
    for (size_t i = row; i + 1 < b->n; i++) b->lines[i] = b->lines[i + 1];
    b->n--;
}

static void clamp_cursor(EditBuf *b) {
    if (b->cur_row >= b->n) b->cur_row = b->n ? b->n - 1 : 0;
    size_t l = strlen(b->lines[b->cur_row]);
    if (b->cur_col > l) b->cur_col = l;
}

EditBuf *edit_create(void) {
    EditBuf *b = calloc(1, sizeof *b);
    if (!b) return NULL;
    if (insert_line(b, 0) != 0) { free(b); return NULL; }
    return b;
}

void edit_free(EditBuf *b) {
    if (!b) return;
    for (size_t i = 0; i < b->n; i++) free(b->lines[i]);
    free(b->lines);
    free(b);
}

int edit_load(EditBuf *b, const char *text) {
    if (!b) return -1;
    b->cur_row = b->cur_col = 0;
    for (size_t i = 0; i < b->n; i++) free(b->lines[i]);
    b->n = 0;

    if (!text || !text[0]) {
        if (insert_line(b, 0) != 0) return -1;
        b->dirty = 0;
        return 0;
    }
    const char *p = text;
    size_t start = 0;
    while (1) {
        const char *nl = strchr(p + start, '\n');
        size_t len = nl ? (size_t)(nl - (p + start)) : strlen(p + start);
        char tmp[65536];
        if (len >= sizeof tmp) len = sizeof tmp - 1;
        memcpy(tmp, p + start, len); tmp[len] = 0;
        if (insert_line(b, b->n) != 0) return -1;
        set_line(b, b->n - 1, tmp);
        if (!nl) break;
        start = (size_t)(nl - p) + 1;
        if (p[start] == 0) { if (insert_line(b, b->n) != 0) return -1; break; }
    }
    b->cur_row = b->cur_col = 0;
    b->dirty = 0;
    return 0;
}

char *edit_serialize(const EditBuf *b) {
    if (!b) return NULL;
    size_t total = 1;
    for (size_t i = 0; i < b->n; i++) total += strlen(b->lines[i]) + 1;
    char *out = malloc(total);
    if (!out) return NULL;
    char *q = out;
    for (size_t i = 0; i < b->n; i++) {
        size_t l = strlen(b->lines[i]);
        memcpy(q, b->lines[i], l); q += l;
        if (i + 1 < b->n) *q++ = '\n';
    }
    *q = 0;
    return out;
}

size_t edit_line_count(const EditBuf *b) { return b ? b->n : 0; }
const char *edit_line(const EditBuf *b, size_t row) {
    if (!b || row >= b->n) return "";
    return b->lines[row];
}
int edit_dirty(const EditBuf *b) { return b ? b->dirty : 0; }
size_t edit_cursor_row(const EditBuf *b) { return b ? b->cur_row : 0; }
size_t edit_cursor_col(const EditBuf *b) { return b ? b->cur_col : 0; }

void edit_cursor_set(EditBuf *b, size_t row, size_t col) {
    if (!b) return;
    b->cur_row = row < b->n ? row : (b->n ? b->n - 1 : 0);
    b->cur_col = col > strlen(b->lines[b->cur_row]) ? strlen(b->lines[b->cur_row]) : col;
}
void edit_cursor_home(EditBuf *b) { if (b) b->cur_col = 0; }
void edit_cursor_end(EditBuf *b)  { if (b) b->cur_col = strlen(b->lines[b->cur_row]); }

void edit_put_char(EditBuf *b, char c) {
    if (!b) return;
    if (c == '\t') c = ' ';
    char *line = b->lines[b->cur_row];
    size_t l = strlen(line), col = b->cur_col;
    char *nw = malloc(l + 2);
    if (!nw) return;
    memcpy(nw, line, col);
    nw[col] = c;
    memcpy(nw + col + 1, line + col, l - col);
    nw[l + 1] = 0;
    free(line); b->lines[b->cur_row] = nw;
    b->cur_col++; b->dirty = 1;
}

void edit_new_line(EditBuf *b) {
    if (!b) return;
    char *line = b->lines[b->cur_row];
    size_t l = strlen(line), col = b->cur_col;
    char left[65536], right[65536];
    memcpy(left, line, col); left[col] = 0;
    memcpy(right, line + col, l - col); right[l - col] = 0;
    if (right[0] == ' ') memmove(right, right + 1, strlen(right + 1) + 1);
    set_line(b, b->cur_row, left);
    if (insert_line(b, b->cur_row + 1) != 0) return;
    set_line(b, b->cur_row + 1, right);
    b->cur_row++; b->cur_col = 0; b->dirty = 1;
}

void edit_backspace(EditBuf *b) {
    if (!b) return;
    if (b->cur_col > 0) {
        char *line = b->lines[b->cur_row];
        size_t l = strlen(line), c = b->cur_col;
        /* delete the char before the cursor: shift [c, l] left by one */
        memmove(line + c - 1, line + c, l - c + 1);
        b->cur_col--; b->dirty = 1;
        return;
    }
    if (b->cur_row == 0) return;
    size_t prev = b->cur_row - 1;
    size_t pl = strlen(b->lines[prev]), ll = strlen(b->lines[b->cur_row]);
    char *nw = malloc(pl + ll + 1);
    if (!nw) return;
    memcpy(nw, b->lines[prev], pl);
    memcpy(nw + pl, b->lines[b->cur_row], ll + 1);
    free(b->lines[prev]); b->lines[prev] = nw;
    remove_line(b, b->cur_row);
    b->cur_row = prev; b->cur_col = pl; b->dirty = 1;
}

void edit_delete(EditBuf *b) {
    if (!b) return;
    char *line = b->lines[b->cur_row];
    size_t l = strlen(line), c = b->cur_col;
    if (c < l) {
        /* delete the char under the cursor: shift [c+1, l] left by one */
        memmove(line + c, line + c + 1, l - c);
        b->dirty = 1; return;
    }
    if (b->cur_row + 1 >= b->n) return;
    size_t nx = b->cur_row + 1;
    size_t nl = strlen(b->lines[nx]);
    char *nw = malloc(l + nl + 1);
    if (!nw) return;
    memcpy(nw, line, l);
    memcpy(nw + l, b->lines[nx], nl + 1);
    free(b->lines[b->cur_row]); b->lines[b->cur_row] = nw;
    remove_line(b, nx);
    b->dirty = 1;
}

void edit_tab(EditBuf *b) { if (b) edit_put_char(b, ' '); }

void edit_arrow_left(EditBuf *b)  { if (b && b->cur_col > 0) b->cur_col--; }
void edit_arrow_right(EditBuf *b) {
    if (b && b->cur_col < strlen(b->lines[b->cur_row])) b->cur_col++;
}
void edit_arrow_up(EditBuf *b) {
    if (!b || b->cur_row == 0) return;
    b->cur_row--; clamp_cursor(b);
}
void edit_arrow_down(EditBuf *b) {
    if (!b || b->cur_row + 1 >= b->n) return;
    b->cur_row++; clamp_cursor(b);
}

void edit_goto_line(EditBuf *b, size_t one_based) {
    if (!b) return;
    if (one_based < 1) one_based = 1;
    if (one_based > b->n) one_based = b->n;
    b->cur_row = one_based - 1;
    b->cur_col = 0;
}

int edit_find_next(EditBuf *b, const char *needle) {
    if (!b || !needle || !needle[0]) return 0;
    size_t start_row = b->cur_row, start_col = b->cur_col;
    for (size_t pass = 0; pass < 2; pass++) {
        size_t r0 = (pass == 0) ? start_row : 0;
        size_t r1 = (pass == 0) ? b->n : start_row + 1;
        for (size_t r = r0; r < r1 && r < b->n; r++) {
            const char *line = b->lines[r];
            size_t from = (pass == 0 && r == start_row) ? start_col : 0;
            const char *hit = strstr(line + from, needle);
            if (hit) {
                b->cur_row = r;
                size_t hc = (size_t)(hit - line);
                b->cur_col = (hc + 1 <= strlen(line)) ? hc + 1 : strlen(line);
                return 1;
            }
        }
    }
    return 0;
}
