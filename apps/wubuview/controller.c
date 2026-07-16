/* controller.c -- pure wubuview interaction logic (viewport + tabs). */
#include "controller.h"

#include <string.h>
#include <stdio.h>

/* viewer command palette (Ctrl+K) -- discoverability without a ribbon.
 * Exposed via controller.h (extern) so the UI (main.c draw_frame) can show
 * the matching-command hint. */
const char *VCMD[] = { "top", "bottom", "pgup", "pgdn", "quit", "nexttab", "prevtab" };

static const char *VB_LABELS[VB__COUNT] = {
    [VB_TOP] = "Top", [VB_PGUP] = "PgUp", [VB_PGDN] = "PgDn",
    [VB_BOT] = "Bot", [VB_QUIT] = "Quit",
};

void vctrl_palette_open(VState *st) {
    st->palette = 1; st->pal_len = 0; st->pal_buf[0] = 0; st->pal_sel = 0;
}

#define PAGE_FRAC 0.9f

static VTab *active(VState *st) { return &st->tabs[st->tab_active]; }

void vctrl_init(VState *st, size_t screen_w, size_t screen_h) {
    memset(st, 0, sizeof *st);
    st->running  = 1;
    st->screen_w = screen_w;
    st->body_y   = 1;
    st->body_h   = (screen_h > 3) ? screen_h - 3 : 1;  /* hdr + footer */
    st->footer_y = (screen_h > 0) ? screen_h - 1 : 0;
    /* footer button layout: right-aligned, one space apart */
    size_t x = screen_w;
    for (int i = (int)VB__COUNT - 1; i >= 0; i--) {
        size_t w = (size_t)strlen(VB_LABELS[i]) + 2;  /* [ label ] */
        if (x < w) { x = 0; break; }
        x -= w;
        st->btn_x[i] = x; st->btn_w[i] = w;
        if (i > 0) x -= 1;  /* gap */
    }
}

void vctrl_resize(VState *st, size_t screen_w, size_t screen_h, size_t total_lines) {
    st->screen_w = screen_w;
    st->body_h   = (screen_h > 3) ? screen_h - 3 : 1;
    st->footer_y = (screen_h > 0) ? screen_h - 1 : 0;
    /* relayout footer buttons */
    size_t x = screen_w;
    for (int i = (int)VB__COUNT - 1; i >= 0; i--) {
        size_t w = (size_t)strlen(VB_LABELS[i]) + 2;
        if (x < w) { x = 0; break; }
        x -= w;
        st->btn_x[i] = x; st->btn_w[i] = w;
        if (i > 0) x -= 1;
    }
    /* update the active tab's wrapped total + clamp its scroll */
    active(st)->total = total_lines;
    size_t max = vctrl_max_scroll(st);
    if (active(st)->scroll > max) active(st)->scroll = max;
}

long vctrl_open(VState *st, const char *title, size_t total_lines) {
    if (st->tab_n >= VCTRL_MAX_TABS) return -1;
    VTab *t = &st->tabs[st->tab_n];
    snprintf(t->title, sizeof t->title, "%.*s", (int)(sizeof t->title - 1), title ? title : "");
    t->total = total_lines; t->scroll = 0;
    int idx = (int)st->tab_n;
    st->tab_n++;
    st->tab_active = (size_t)idx;   /* opening a doc focuses it */
    return idx;
}

size_t vctrl_close(VState *st, size_t idx) {
    if (st->tab_n == 0 || idx >= st->tab_n) return st->tab_active;
    for (size_t i = idx; i + 1 < st->tab_n; i++) st->tabs[i] = st->tabs[i + 1];
    st->tab_n--;
    if (st->tab_active >= st->tab_n) st->tab_active = st->tab_n ? st->tab_n - 1 : 0;
    if (st->tab_n == 0) st->running = 0;  /* last tab closed -> quit */
    return st->tab_active;
}

size_t vctrl_switch(VState *st, int dir) {
    if (st->tab_n <= 1) return st->tab_active;
    long a = (long)st->tab_active + dir;
    if (a < 0) a = (long)st->tab_n - 1;
    if (a >= (long)st->tab_n) a = 0;
    st->tab_active = (size_t)a;
    return st->tab_active;
}

size_t vctrl_max_scroll(const VState *st) {
    const VTab *t = &st->tabs[st->tab_active];
    return t->total > st->body_h ? t->total - st->body_h : 0;
}

VBtn vctrl_button_at(const VState *st, size_t px, size_t py) {
    if (py != st->footer_y) return VB__COUNT;
    for (int i = 0; i < (int)VB__COUNT; i++)
        if (px >= st->btn_x[i] && px < st->btn_x[i] + st->btn_w[i])
            return (VBtn)i;
    return VB__COUNT;
}

VBtn vctrl_handle(VState *st, const TuiKey *k) {
    VBtn clicked = VB__COUNT;
    VTab *t = active(st);

    if (k->type == TUI_KEY_MOUSE) {
        size_t mx = k->mouse_x, my = k->mouse_y, mb = k->mouse_button;
        int drag = (mb == TUI_MBTN_LEFT);  /* left button */

        if (k->mouse_action == TUI_MOUSE_WHEEL_UP) { t->scroll = t->scroll > 3 ? t->scroll - 3 : 0; return VB__COUNT; }
        if (k->mouse_action == TUI_MOUSE_WHEEL_DOWN) {
            size_t max = vctrl_max_scroll(st);
            t->scroll = (t->scroll + 3 < max) ? t->scroll + 3 : max;
            return VB__COUNT;
        }

        if (k->mouse_action == TUI_MOUSE_RELEASE) { st->dragging = 0; return VB__COUNT; }  /* release ends drag */

        /* scrollbar drag (right edge, one column) */
        size_t sb = st->screen_w > 0 ? st->screen_w - 1 : 0;
        if (drag && (st->dragging || mx == sb) && my >= st->body_y && my < st->body_y + st->body_h) {
            if (mx == sb || st->dragging) {
                st->dragging = 1;
                size_t row = my - st->body_y;
                size_t max = vctrl_max_scroll(st);
                t->scroll = max ? (row * max) / (st->body_h - 1) : 0;
                return VB__COUNT;
            }
        }

        /* footer button click */
        if (drag && my == st->footer_y) {
            VBtn b = vctrl_button_at(st, mx, my);
            if (b != VB__COUNT) { clicked = b; goto act; }
        }
        return VB__COUNT;
    }

    /* command palette (Ctrl+K) mode */
    if (st->palette) {
        if (k->type == TUI_KEY_CHAR) {
            if (k->ch == 0x0d || k->ch == 0x0a) {        /* Enter runs */
                if (st->pal_len) {
                    /* match the typed prefix to a command */
                    int run = -1;
                    for (int i = 0; i < VCMD_N; i++)
                        if (strncmp(VCMD[i], st->pal_buf, st->pal_len) == 0) { run = i; break; }
                    if (run >= 0) {
                        st->palette = 0; st->pal_len = 0;
                        VTab *t = active(st);
                        switch (run) {
                            case 0: t->scroll = 0; break;
                            case 1: t->scroll = vctrl_max_scroll(st); break;
                            case 2: if (t->scroll > st->body_h - 1) t->scroll -= (st->body_h - 1); else t->scroll = 0; break;
                            case 3: { size_t mx = vctrl_max_scroll(st); t->scroll = (t->scroll + (st->body_h > 1 ? st->body_h - 1 : 1) < mx) ? t->scroll + (st->body_h > 1 ? st->body_h - 1 : 1) : mx; } break;
                            case 4: st->running = 0; break;
                            case 5: vctrl_switch(st, +1); break;
                            case 6: vctrl_switch(st, -1); break;
                        }
                        return VB__COUNT;
                    }
                }
                st->palette = 0; st->pal_len = 0; return VB__COUNT;
            }
            if (k->ch == 0x1b) { st->palette = 0; st->pal_len = 0; return VB__COUNT; }
            if (k->ch == 0x7f || k->ch == 0x08) { if (st->pal_len) st->pal_len--; return VB__COUNT; }
            if (st->pal_len + 1 < sizeof st->pal_buf && k->ch >= 0x20) {
                st->pal_buf[st->pal_len++] = (char)k->ch; st->pal_buf[st->pal_len] = 0;
            }
            return VB__COUNT;
        }
        if (k->type == TUI_KEY_UP && st->pal_sel > 0) st->pal_sel--;
        else if (k->type == TUI_KEY_DOWN && st->pal_sel + 1 < VCMD_N) st->pal_sel++;
        return VB__COUNT;
    }

    /* keyboard / char */
    switch (k->type) {
        case TUI_KEY_CHAR:
            switch (k->ch) {
                case 'q': case 'Q': case 0x1b: st->running = 0; return VB__COUNT;
                case 'j': case 'J': case ' ': case 0x0c: {  /* down / pgdn */
                    size_t pg = (size_t)(st->body_h * PAGE_FRAC);
                    (void)pg;
                    if (k->ch == ' ' || k->ch == 0x0c) t->scroll += (st->body_h > 1 ? st->body_h - 1 : 1);
                    else t->scroll += 1;
                    size_t max = vctrl_max_scroll(st);
                    if (t->scroll > max) t->scroll = max;
                    return VB__COUNT;
                }
                case 'k': case 'K': case 0x0b:  /* up / pgup / Ctrl+K */
                    if (k->ch == 0x0b) { vctrl_palette_open(st); return VB__COUNT; }
                    t->scroll = t->scroll ? t->scroll - 1 : 0;
                    return VB__COUNT;
                case 'g': case 0x01:  /* home */
                    if (k->ch == 0x01) { /* Ctrl-A unused; plain g */ }
                    t->scroll = 0; return VB__COUNT;
                case 'G': case 0x05:  /* end */
                    t->scroll = vctrl_max_scroll(st); return VB__COUNT;
                case 0x02:  /* Ctrl-B (page up) */
                    if (t->scroll > st->body_h - 1) t->scroll -= (st->body_h - 1); else t->scroll = 0;
                    return VB__COUNT;
                case 0x06:  /* Ctrl-F (page down) */
                    t->scroll += (st->body_h > 1 ? st->body_h - 1 : 1);
                    { size_t max = vctrl_max_scroll(st); if (t->scroll > max) t->scroll = max; }
                    return VB__COUNT;
                case 0x09:  /* Ctrl-I / next tab */
                    vctrl_switch(st, +1); return VB__COUNT;
                case 0x15:  /* Ctrl-U / prev tab */
                    vctrl_switch(st, -1); return VB__COUNT;
                default: return VB__COUNT;
            }
            break;
        case TUI_KEY_UP:
            t->scroll = t->scroll ? t->scroll - 1 : 0; return VB__COUNT;
        case TUI_KEY_DOWN: {
            size_t max = vctrl_max_scroll(st);
            t->scroll = (t->scroll + 1 < max) ? t->scroll + 1 : max;
            return VB__COUNT;
        }
        case TUI_KEY_PAGE_UP:
            if (t->scroll > st->body_h - 1) t->scroll -= (st->body_h - 1); else t->scroll = 0;
            return VB__COUNT;
        case TUI_KEY_PAGE_DOWN:
            t->scroll += (st->body_h > 1 ? st->body_h - 1 : 1);
            { size_t max = vctrl_max_scroll(st); if (t->scroll > max) t->scroll = max; }
            return VB__COUNT;
        case TUI_KEY_HOME: t->scroll = 0; return VB__COUNT;
        case TUI_KEY_END:  t->scroll = vctrl_max_scroll(st); return VB__COUNT;
        default: return VB__COUNT;
    }

act:
    switch (clicked) {
        case VB_TOP:  t->scroll = 0; break;
        case VB_PGUP: if (t->scroll > st->body_h - 1) t->scroll -= (st->body_h - 1); else t->scroll = 0; break;
        case VB_PGDN: { size_t max = vctrl_max_scroll(st); t->scroll = (t->scroll + (st->body_h > 1 ? st->body_h - 1 : 1) < max) ? t->scroll + (st->body_h > 1 ? st->body_h - 1 : 1) : max; } break;
        case VB_BOT:  t->scroll = vctrl_max_scroll(st); break;
        case VB_QUIT: st->running = 0; break;
        default: break;
    }
    return clicked;
}
