/* controller.c -- pure wubuview interaction logic (no TTY, fully testable). */
#include "controller.h"

#include <string.h>
#include <stdio.h>

/* Footer button labels, in left-to-right order. Keep in sync with main.c's
 * draw code; the controller owns the hit-box math, main owns the glyphs. */
static const char *VB_LABELS[VB__COUNT] = {
    [VB_TOP]  = "Top",
    [VB_PGUP] = "PgUp",
    [VB_PGDN] = "PgDn",
    [VB_BOT]  = "Bot",
    [VB_QUIT] = "Quit",
};

static size_t vb_width(VBtn b) {
    const char *l = VB_LABELS[b];
    return (l ? strlen(l) : 0) + 4;   /* "[ " + label + " ]" */
}

void vctrl_init(VState *st, size_t screen_w, size_t screen_h, size_t total_lines) {
    memset(st, 0, sizeof *st);
    st->total = total_lines;
    st->running = 1;
    vctrl_resize(st, screen_w, screen_h, total_lines);
}

void vctrl_resize(VState *st, size_t screen_w, size_t screen_h, size_t total_lines) {
    if (screen_w < 4) screen_w = 4;
    if (screen_h < 3) screen_h = 3;
    st->screen_w = screen_w;
    st->body_h   = screen_h - 2;   /* header + footer */
    st->body_y   = 1;
    st->total    = total_lines;
    st->footer_y = screen_h - 1;

    /* lay out footer buttons after the position readout */
    char pos[64];
    int pct = total_lines ? (int)((st->body_h * 100) / total_lines) : 100;
    size_t shown = st->body_h < total_lines ? st->body_h : total_lines;
    snprintf(pos, sizeof pos, " %zu-%zu/%zu (%d%%)  ",
             total_lines ? st->scroll + 1 : 0, shown, total_lines, pct);
    size_t x = strlen(pos);
    for (int i = 0; i < (int)VB__COUNT; i++) {
        st->btn_x[i] = x;
        st->btn_w[i] = vb_width((VBtn)i);
        x += st->btn_w[i] + 1;
    }

    /* clamp scroll to the new bounds */
    size_t mx = vctrl_max_scroll(st);
    if (st->scroll > mx) st->scroll = mx;
}

size_t vctrl_max_scroll(const VState *st) {
    return (st->total > st->body_h) ? st->total - st->body_h : 0;
}

VBtn vctrl_button_at(const VState *st, size_t px, size_t py) {
    if (py != st->footer_y) return VB__COUNT;
    for (int i = 0; i < (int)VB__COUNT; i++) {
        size_t x = st->btn_x[i], w = st->btn_w[i];
        if (px >= x && px < x + w) return (VBtn)i;
    }
    return VB__COUNT;
}

VBtn vctrl_handle(VState *st, const TuiKey *k) {
    if (!st || !k) return VB__COUNT;
    size_t mx = vctrl_max_scroll(st);
    VBtn clicked = VB__COUNT;

    switch (k->type) {
        case TUI_KEY_CHAR:
            if (k->ch == 'q' || k->ch == 'Q') st->running = 0;
            else if (k->ch == 'j') { if (st->scroll < mx) st->scroll++; }
            else if (k->ch == 'k') { if (st->scroll) st->scroll--; }
            else if (k->ch == ' ') st->scroll = (st->scroll + st->body_h < mx) ? st->scroll + st->body_h : mx;
            else if (k->ch == 'b') st->scroll = (st->scroll > st->body_h) ? st->scroll - st->body_h : 0;
            else if (k->ch == 'g') st->scroll = 0;
            else if (k->ch == 'G') st->scroll = mx;
            break;
        case TUI_KEY_ESC:  st->running = 0; break;
        case TUI_KEY_DOWN:      if (st->scroll < mx) st->scroll++; break;
        case TUI_KEY_UP:        if (st->scroll) st->scroll--; break;
        case TUI_KEY_PAGE_DOWN: st->scroll = (st->scroll + st->body_h < mx) ? st->scroll + st->body_h : mx; break;
        case TUI_KEY_PAGE_UP:   st->scroll = (st->scroll > st->body_h) ? st->scroll - st->body_h : 0; break;
        case TUI_KEY_HOME:      st->scroll = 0; break;
        case TUI_KEY_END:       st->scroll = mx; break;
        case TUI_KEY_MOUSE: {
            size_t x = k->mouse_x, y = k->mouse_y;
            if (k->mouse_action == TUI_MOUSE_WHEEL_UP) {
                st->scroll = (st->scroll > 3) ? st->scroll - 3 : 0;
            } else if (k->mouse_action == TUI_MOUSE_WHEEL_DOWN) {
                st->scroll = (st->scroll + 3 < mx) ? st->scroll + 3 : mx;
            } else if (k->mouse_action == TUI_MOUSE_PRESS && k->mouse_button == TUI_MBTN_LEFT) {
                /* footer button? */
                VBtn b = vctrl_button_at(st, x, y);
                if (b != VB__COUNT) {
                    clicked = b;
                    switch (b) {
                        case VB_TOP:  st->scroll = 0; break;
                        case VB_PGUP: st->scroll = (st->scroll > st->body_h) ? st->scroll - st->body_h : 0; break;
                        case VB_PGDN: st->scroll = (st->scroll + st->body_h < mx) ? st->scroll + st->body_h : mx; break;
                        case VB_BOT:  st->scroll = mx; break;
                        case VB_QUIT: st->running = 0; break;
                        default: break;
                    }
                } else if (x == st->screen_w - 1 && y >= st->body_y && y < st->body_y + st->body_h) {
                    /* scrollbar click -> jump */
                    size_t row = y - st->body_y;
                    size_t track = st->body_h;
                    if (track > 1) st->scroll = (row * mx) / (track - 1);
                    if (st->scroll > mx) st->scroll = mx;
                    st->dragging = 1;
                }
            } else if (k->mouse_action == TUI_MOUSE_DRAG && st->dragging) {
                size_t row = (y >= st->body_y) ? y - st->body_y : 0;
                if (st->body_h && row >= st->body_h) row = st->body_h - 1;
                size_t track = st->body_h;
                if (track > 1) st->scroll = (row * mx) / (track - 1);
                if (st->scroll > mx) st->scroll = mx;
            } else if (k->mouse_action == TUI_MOUSE_RELEASE) {
                st->dragging = 0;
            }
            break;
        }
        default: break;
    }
    return clicked;
}
