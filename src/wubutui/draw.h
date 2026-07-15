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

#ifdef __cplusplus
}
#endif

#endif /* WUBUTUI_DRAW_H */
