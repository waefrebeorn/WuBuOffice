/* note_controller.c -- pure wubunote interaction logic. */
#include "note_controller.h"

#include "draw.h"   /* TuiTab / tui_tabbar_hit */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* command palette entries (Ctrl+K) */
const char *NCMD[] = {
    ":w", ":q", ":tabnew", ":tabclose", ":wrap", ":find", ":goto"
};
const char *NCMD_HELP[] = {
    "save", "quit", "new tab", "close tab", "toggle wrap", "find", "goto line"
};
#define NCMD_N ((int)(sizeof NCMD / sizeof NCMD[0]))

static EditBuf *active_buf(NState *st) { return st->tabs[st->tab_active].buf; }

EditBuf *nctrl_active_buf(const NState *st) { return st->tabs[st->tab_active].buf; }

static void layout(NState *st, size_t w, size_t h) {
    st->screen_w = w; st->screen_h = h;
    st->body_y   = 2;                          /* header + tabbar */
    st->body_h   = (h > 4) ? h - 4 : 1; /* hdr+tabbar+status */
    st->gutter   = st->wrap ? 0 : 5;        /* 4-digit gutter + space */
}

void nctrl_init(NState *st, size_t w, size_t h) {
    memset(st, 0, sizeof *st);
    st->running = 1;
    st->wrap    = 0;     /* Notepad default: no wrap, scroll horizontally */
    layout(st, w, h);
}

void nctrl_resize(NState *st, size_t w, size_t h) { layout(st, w, h); }

int nctrl_open(NState *st, const char *path) {
    if (st->tab_n >= NCTRL_MAX_TABS) return -1;
    NTab *t = &st->tabs[st->tab_n];
    t->buf = edit_create();
    if (!t->buf) return -1;
    if (path && path[0]) {
        snprintf(t->path, sizeof t->path, "%.*s", (int)(sizeof t->path - 1), path);
        /* best-effort load; missing file -> empty doc (new) */
        FILE *f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t rd = fread(buf, 1, (size_t)sz, f);
                buf[rd] = 0;
                edit_load(t->buf, buf);
                free(buf);
            }
            fclose(f);
        }
    } else {
        snprintf(t->path, sizeof t->path, "untitled-%zu", st->tab_n + 1);
    }
    int idx = (int)st->tab_n;
    st->tab_n++;
    st->tab_active = (size_t)idx;
    return idx;
}

int nctrl_new(NState *st) { return nctrl_open(st, NULL); }

void nctrl_close_active(NState *st) {
    if (st->tab_n == 0) { st->running = 0; return; }
    size_t i = st->tab_active;
    edit_free(st->tabs[i].buf);
    for (; i + 1 < st->tab_n; i++) st->tabs[i] = st->tabs[i + 1];
    st->tab_n--;
    if (st->tab_active >= st->tab_n) st->tab_active = st->tab_n ? st->tab_n - 1 : 0;
    if (st->tab_n == 0) st->running = 0;
}

void nctrl_switch(NState *st, int dir) {
    if (st->tab_n <= 1) return;
    long a = (long)st->tab_active + dir;
    if (a < 0) a = (long)st->tab_n - 1;
    if (a >= (long)st->tab_n) a = 0;
    st->tab_active = (size_t)a;
}

void nctrl_prompt_find(NState *st) {
    st->prompt = NPMT_FIND; st->prompt_len = 0; st->prompt_buf[0] = 0;
    st->prompt_label = "Find:";
}
void nctrl_prompt_goto(NState *st) {
    st->prompt = NPMT_GOTO; st->prompt_len = 0; st->prompt_buf[0] = 0;
    st->prompt_label = "Goto:";
}
void nctrl_prompt_saveas(NState *st) {
    st->prompt = NPMT_SAVEAS; st->prompt_len = 0; st->prompt_buf[0] = 0;
    st->prompt_label = "Save:";
    /* start empty; the current path is offered by the :w palette command */
}
void nctrl_prompt_cmd(NState *st) {
    st->prompt = NPMT_CMD; st->prompt_len = 0; st->prompt_buf[0] = 0;
    st->prompt_label = ":";
    st->cmd_sel = 0;
}

int nctrl_prompt_commit(NState *st, char **out_path) {
    if (st->prompt == NPMT_NONE) return 0;
    int did = 1;
    switch (st->prompt) {
        case NPMT_FIND:
            if (st->prompt_len) edit_find_next(active_buf(st), st->prompt_buf);
            break;
        case NPMT_GOTO: {
            size_t ln = 0;
            for (size_t i = 0; i < st->prompt_len; i++)
                if (st->prompt_buf[i] >= '0' && st->prompt_buf[i] <= '9')
                    ln = ln * 10 + (size_t)(st->prompt_buf[i] - '0');
            edit_goto_line(active_buf(st), ln);
            break;
        }
        case NPMT_SAVEAS:
            if (st->prompt_len && out_path) {
                *out_path = malloc(st->prompt_len + 1);
                if (*out_path) { memcpy(*out_path, st->prompt_buf, st->prompt_len); (*out_path)[st->prompt_len] = 0; }
                /* remember the new path on the tab (save-in-place next time) */
                snprintf(st->tabs[st->tab_active].path,
                         sizeof st->tabs[st->tab_active].path,
                         "%.*s", (int)(sizeof st->tabs[st->tab_active].path - 1),
                         st->prompt_buf);
            }
            break;
        case NPMT_CMD: {
            /* match the typed prefix to a command, then run it */
            int run = -1;
            for (int i = 0; i < NCMD_N; i++)
                if (strncmp(NCMD[i], st->prompt_buf, st->prompt_len) == 0) { run = i; break; }
            if (run < 0) { st->prompt = NPMT_NONE; return 0; }
            switch (run) {
                case 0: if (out_path) { const char *p = st->tabs[st->tab_active].path; size_t l=strlen(p); *out_path=malloc(l+1); if(*out_path){memcpy(*out_path,p,l);(*out_path)[l]=0;} } break;
                case 1: st->running = 0; break;
                case 2: nctrl_new(st); break;
                case 3: nctrl_close_active(st); break;
                case 4: st->wrap = st->wrap ? 0 : 1; layout(st, st->screen_w, st->screen_h); break;
                case 5: nctrl_prompt_find(st); return 1; /* re-enter find prompt */
                case 6: nctrl_prompt_goto(st); return 1; /* re-enter goto prompt */
            }
            break;
        }
        default: did = 0; break;
    }
    st->prompt = NPMT_NONE;
    st->prompt_len = 0;
    return did;
}

/* handle one key while a prompt is open (edits prompt_buf) */
static void prompt_key(NState *st, const TuiKey *k) {
    if (k->type == TUI_KEY_CHAR) {
        if (k->ch == 0x0d || k->ch == 0x0a) return; /* commit handled by caller */
        if (k->ch == 0x1b) { st->prompt = NPMT_NONE; st->prompt_len = 0; return; }
        if (k->ch == 0x7f || k->ch == 0x08) { /* backspace */
            if (st->prompt_len) st->prompt_len--;
            if (st->prompt == NPMT_CMD) st->cmd_sel = 0;
            return;
        }
        if (st->prompt_len + 1 < NCTRL_PROMPT_MAX && k->ch >= 0x20) {
            st->prompt_buf[st->prompt_len++] = (char)k->ch;
            st->prompt_buf[st->prompt_len] = 0;
        }
    } else if (k->type == TUI_KEY_BACKSPACE) {
        if (st->prompt_len) st->prompt_len--;
    } else if (k->type == TUI_KEY_LEFT) {
        if (st->cmd_sel > 0) st->cmd_sel--;
    } else if (k->type == TUI_KEY_RIGHT || k->type == TUI_KEY_DOWN) {
        if (st->cmd_sel + 1 < NCMD_N) st->cmd_sel++;
    } else if (k->type == TUI_KEY_UP) {
        if (st->cmd_sel > 0) st->cmd_sel--;
    }
}

void nctrl_handle(NState *st, const TuiKey *k, char **out_path) {
    if (out_path) *out_path = NULL;
    if (!st->running) return;

    /* mouse: click a tab to switch */
    if (k->type == TUI_KEY_MOUSE && k->mouse_action == TUI_MOUSE_PRESS
        && k->mouse_button == TUI_MBTN_LEFT && k->mouse_y == 1) {
        TuiTab ht[NCTRL_MAX_TABS];
        for (size_t i = 0; i < st->tab_n; i++) {
            ht[i].label = st->tabs[i].path;
            ht[i].active = (i == st->tab_active);
            ht[i].x = ht[i].w = 0;
        }
        int hit = tui_tabbar_hit(ht, st->tab_n, k->mouse_x, 1, 1);
        if (hit >= 0) st->tab_active = (size_t)hit;
        return;
    }

    if (st->prompt != NPMT_NONE) {
        if (k->type == TUI_KEY_CHAR && (k->ch == 0x0d || k->ch == 0x0a))
            nctrl_prompt_commit(st, out_path);
        else
            prompt_key(st, k);
        return;
    }

    /* normal editing */
    switch (k->type) {
        case TUI_KEY_CHAR:
            switch (k->ch) {
                case 0x1b: st->running = 0; return;          /* Esc quits */
                case 0x0d: case 0x0a: edit_new_line(active_buf(st)); return;
                case 0x09: edit_tab(active_buf(st)); return;          /* Tab */
                case 0x7f: case 0x08: edit_backspace(active_buf(st)); return;
                case 0x0b: nctrl_prompt_cmd(st); return;            /* Ctrl-K command palette */
                case 0x06: nctrl_prompt_find(st); return;          /* Ctrl-F find */
                case 0x07: nctrl_prompt_goto(st); return;          /* Ctrl-G goto */
                case 0x13: { if (out_path) { const char *p=st->tabs[st->tab_active].path; size_t l=strlen(p); *out_path=malloc(l+1); if(*out_path){memcpy(*out_path,p,l);(*out_path)[l]=0;} } return; } /* Ctrl-S save */
                case 0x17: st->wrap = st->wrap ? 0 : 1; layout(st, st->screen_w, st->screen_h); return; /* Ctrl-W wrap */
                case 0x02: nctrl_switch(st, -1); return;       /* Ctrl-B prev tab */
                case 0x05: nctrl_switch(st, +1); return;       /* Ctrl-E next tab (avoid Ctrl-I=Tab) */
                default:
                    if (k->ch >= 0x20) edit_put_char(active_buf(st), (char)k->ch);
                    return;
            }
        case TUI_KEY_ENTER:  edit_new_line(active_buf(st)); return;
        case TUI_KEY_BACKSPACE: edit_backspace(active_buf(st)); return;
        case TUI_KEY_TAB: edit_tab(active_buf(st)); return;
        case TUI_KEY_LEFT:  edit_arrow_left(active_buf(st)); return;
        case TUI_KEY_RIGHT: edit_arrow_right(active_buf(st)); return;
        case TUI_KEY_UP:    edit_arrow_up(active_buf(st)); return;
        case TUI_KEY_DOWN:  edit_arrow_down(active_buf(st)); return;
        case TUI_KEY_HOME:  edit_cursor_home(active_buf(st)); return;
        case TUI_KEY_END:   edit_cursor_end(active_buf(st)); return;
        case TUI_KEY_DELETE: edit_delete(active_buf(st)); return;
        default: return;
    }
}

size_t nctrl_wrapped_total(const NState *st, const char *text, size_t text_len) {
    (void)st; (void)text; (void)text_len;
    /* callers compute wrap via tui_wrap; this is a convenience stub */
    return 0;
}
