/* draw.h -- wubutui drawing primitives over a TuiScreen.
 *
 * All primitives are pure screen writes (clipped to the screen). The word-wrap
 * helper is separated out as a pure function so it can be tested without a
 * screen at all: given a paragraph and a width it produces the wrapped lines.
 */
#ifndef WUBUTUI_DRAW_H
#define WUBUTUI_DRAW_H

#include "screen.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Write a NUL-terminated string starting at (x,y), left to right, clipped to
 * the screen edge. Newlines are NOT interpreted (use the wrap helpers). */
void tui_text(TuiScreen *s, size_t x, size_t y, const char *str, uint8_t attr);

/* Fill a horizontal run of `n` cells from (x,y) with `ch`. */
void tui_hline(TuiScreen *s, size_t x, size_t y, size_t n, char ch, uint8_t attr);
/* Fill a vertical run of `n` cells from (x,y) with `ch`. */
void tui_vline(TuiScreen *s, size_t x, size_t y, size_t n, char ch, uint8_t attr);

/* Draw an ASCII box (+, -, |) with top-left (x,y) of size w*h (w,h >= 2). */
void tui_box(TuiScreen *s, size_t x, size_t y, size_t w, size_t h, uint8_t attr);

/* Fill a rectangle with `ch`. */
void tui_fill(TuiScreen *s, size_t x, size_t y, size_t w, size_t h, char ch, uint8_t attr);

/* --- pure word wrap (no screen) ---
 * Wrap `text` (a single paragraph; embedded '\n' force line breaks) into lines
 * no wider than `width` columns, breaking on spaces where possible and hard-
 * breaking words longer than `width`. Returns a malloc'd array of malloc'd
 * NUL-terminated line strings and sets *out_count. Caller frees each line then
 * the array (or use tui_wrap_free). Returns NULL on OOM or width==0. */
char **tui_wrap(const char *text, size_t width, size_t *out_count);
void   tui_wrap_free(char **lines, size_t count);

/* Draw wrapped text into the rectangle (x,y,w,h), starting at paragraph line
 * `scroll` (for vertical scrolling). Returns the total number of wrapped lines
 * the text occupies (so the caller can compute scroll bounds), or 0 on error. */
size_t tui_text_wrapped(TuiScreen *s, size_t x, size_t y, size_t w, size_t h,
                        const char *text, size_t scroll, uint8_t attr);

/* --- vertical scrollbar widget ---
 * A proportional scrollbar in a 1-column track of height `h` at (x,y). The
 * thumb size reflects viewport/content ratio and its position reflects
 * `scroll`. `total` is the content line count, `visible` the viewport height.
 * Pure geometry so it is testable: tui_scrollbar_thumb() computes the thumb's
 * [start,len) within the track without drawing. */
typedef struct { size_t start; size_t len; } TuiThumb;

/* Compute the thumb span (0-based offset into an h-cell track, and its length)
 * for the given content/viewport/scroll. Guarantees 1 <= len <= h and keeps the
 * thumb visible at both ends. If everything fits (total <= visible) len == h. */
TuiThumb tui_scrollbar_thumb(size_t h, size_t total, size_t visible, size_t scroll);

/* Draw the scrollbar track ('|') with a highlighted thumb (reverse blocks). */
void tui_scrollbar(TuiScreen *s, size_t x, size_t y, size_t h,
                   size_t total, size_t visible, size_t scroll);

/* Map a click at track-row `row` (0-based within the track) back to a scroll
 * offset, so clicking the track jumps there. Returns a clamped scroll value. */
size_t tui_scrollbar_scroll_at(size_t h, size_t total, size_t visible, size_t row);

/* --- button widget ---
 * A labelled clickable button: "[ Label ]" drawn at (x,y). tui_button() draws
 * it and returns its width in cells. tui_hit() is a generic rectangle hit-test
 * used for buttons and any other clickable region. */
size_t tui_button_width(const char *label);
size_t tui_button(TuiScreen *s, size_t x, size_t y, const char *label, uint8_t attr);

/* Return 1 if point (px,py) lies inside the rect (x,y,w,h), else 0. */
int tui_hit(size_t px, size_t py, size_t x, size_t y, size_t w, size_t h);

#ifdef __cplusplus
}
#endif

#endif /* WUBUTUI_DRAW_H */
