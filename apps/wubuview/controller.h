/* controller.h -- pure wubuview interaction logic.
 *
 * The interactive loop in main.c owns only TTY I/O (read bytes, paint screen).
 * ALL state transitions -- how a key or mouse event changes the scroll offset
 * or which button is hit -- live here as a PURE function with NO TTY, NO global
 * state. That makes the real interaction path unit-testable: feed events, assert
 * the resulting scroll/quit, instead of poking at main's private locals.
 *
 * The state struct carries everything an event handler needs; the caller updates
 * it after each event. This keeps main.c a thin, dumb driver.
 */
#ifndef WUBUVIEW_CONTROLLER_H
#define WUBUVIEW_CONTROLLER_H

#include <stddef.h>
#include <stdbool.h>

#include "screen.h"   /* for TUI_* button/widget draw helpers if needed */
#include "input.h"    /* TuiKey */

#ifdef __cplusplus
extern "C" {
#endif

/* Footer button identifiers (kept in sync with main.c's layout). */
typedef enum {
    VB_TOP = 0, VB_PGUP, VB_PGDN, VB_BOT, VB_QUIT, VB__COUNT
} VBtn;

/* Mutable view state. Recomputed each frame from the terminal size. */
typedef struct {
    size_t total;     /* content line count (wrapped) */
    size_t body_h;    /* viewport height in rows */
    size_t body_y;    /* viewport top row (header is row 0) */
    size_t scroll;    /* current first visible line */
    int    running;   /* 1 while the viewer should stay open */
    int    dragging;  /* 1 while a left-drag on the scrollbar is active */

    /* footer button hit-boxes (columns), filled by vctrl_layout_buttons() */
    size_t btn_x[VB__COUNT];
    size_t btn_w[VB__COUNT];
    size_t footer_y;
    size_t screen_w;
} VState;

/* Initialize state for a given terminal size and content height. */
void vctrl_init(VState *st, size_t screen_w, size_t screen_h,
                size_t total_lines);

/* Recompute derived fields after a resize (keeps scroll clamped). */
void vctrl_resize(VState *st, size_t screen_w, size_t screen_h,
                   size_t total_lines);

/* Maximum scroll offset (total - body_h, or 0 if it all fits). */
size_t vctrl_max_scroll(const VState *st);

/* Apply one decoded key/mouse event to the state. Returns the button id that was
 * clicked this event (VB__COUNT if none) so the caller can, e.g., repaint a
 * pressed button. Mutates st->scroll / st->running / st->dragging. */
VBtn vctrl_handle(VState *st, const TuiKey *k);

/* Pure mapping: which footer button (if any) contains cell (px,py). */
VBtn vctrl_button_at(const VState *st, size_t px, size_t py);

#ifdef __cplusplus
}
#endif

#endif /* WUBUVIEW_CONTROLLER_H */
