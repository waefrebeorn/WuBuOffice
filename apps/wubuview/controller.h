/* controller.h -- pure wubuview interaction logic (viewport + tabs).
 *
 * main.c owns only TTY I/O. ALL state transitions -- scroll, which footer
 * button is hit, tab switching -- live here as PURE functions with NO TTY
 * and NO global state, so the real interaction path is unit-testable.
 */
#ifndef WUBUVIEW_CONTROLLER_H
#define WUBUVIEW_CONTROLLER_H

#include <stddef.h>
#include <stdbool.h>

#include "screen.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* footer button identifiers (kept in sync with main.c's layout) */
typedef enum {
    VB_TOP = 0, VB_PGUP, VB_PGDN, VB_BOT, VB_QUIT, VB__COUNT
} VBtn;

#define VCTRL_MAX_TABS 8

/* a single open document tab */
typedef struct {
    char   title[64];
    size_t total;   /* wrapped line count for the current width */
    size_t scroll;  /* first visible line */
} VTab;

/* mutable view state, recomputed each frame from the terminal size */
typedef struct {
    size_t body_h;    /* viewport height in rows */
    size_t body_y;    /* viewport top row (header is row 0) */
    int    running;   /* 1 while the viewer should stay open */
    int    dragging;  /* 1 while a left-drag on the scrollbar is active */

    /* footer button hit-boxes */
    size_t btn_x[VB__COUNT];
    size_t btn_w[VB__COUNT];
    size_t footer_y;
    size_t screen_w;

    /* tabs */
    VTab   tabs[VCTRL_MAX_TABS];
    size_t tab_n;
    size_t tab_active;

    /* command palette (Ctrl+K) -- discoverability without a ribbon */
    int    palette;       /* 1 while the palette prompt is open */
    size_t pal_len;
    char   pal_buf[64];
    int    pal_sel;
} VState;

void vctrl_init(VState *st, size_t screen_w, size_t screen_h);

/* recompute derived viewport fields + layout for a resize; total_lines is the
 * wrapped count of the ACTIVE tab at the new width. */
void vctrl_resize(VState *st, size_t screen_w, size_t screen_h,
                  size_t total_lines);

/* open the Ctrl+K command palette */
void vctrl_palette_open(VState *st);

/* close tab `idx`; clamps the active index. returns the new active index. */
size_t vctrl_close(VState *st, size_t idx);

/* switch to an adjacent tab (+1 / -1), wrapping. returns active index. */
size_t vctrl_switch(VState *st, int dir);

size_t vctrl_max_scroll(const VState *st);

/* apply one decoded key/mouse event. returns the footer button clicked this
 * event (VB__COUNT if none). mutates st in place. */
VBtn vctrl_handle(VState *st, const TuiKey *k);

/* pure mapping: which footer button (if any) contains cell (px,py) */
VBtn vctrl_button_at(const VState *st, size_t px, size_t py);

#ifdef __cplusplus
}
#endif

#endif /* WUBUVIEW_CONTROLLER_H */
