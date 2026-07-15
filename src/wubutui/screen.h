/* screen.h -- wubutui off-screen cell grid: the render target for native TUIs.
 *
 * A TuiScreen is a WxH grid of cells (character + attribute). All drawing hits
 * this pure in-memory buffer; nothing here touches a terminal, so the whole
 * render path is deterministic and unit-testable without a TTY. Two consumers:
 *   - tui_screen_dump():  plain-text snapshot for tests / headless diffing.
 *   - tui_screen_render(): emit an ANSI byte stream (full or minimal diff) that
 *                          a real terminal displays (see term.{h,c}).
 *
 * Opaque struct (soul.md sec.10): callers touch cells only through the API.
 * ASCII / single-byte cells only (a from-scratch UTF-8 cell model is a later
 * concern; office text renders fine in the ASCII+Latin path for now).
 */
#ifndef WUBUTUI_SCREEN_H
#define WUBUTUI_SCREEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cell attributes: a small, terminal-portable set (SGR subset). Combine with
 * bitwise OR. TUI_ATTR_NONE is the default (no styling). */
enum {
    TUI_ATTR_NONE    = 0,
    TUI_ATTR_BOLD    = 1u << 0,
    TUI_ATTR_REVERSE = 1u << 1,
    TUI_ATTR_DIM     = 1u << 2,
    TUI_ATTR_UNDERLINE = 1u << 3
};

typedef struct TuiScreen TuiScreen;

/* Create a WxH screen filled with spaces / TUI_ATTR_NONE. NULL on OOM or a
 * zero / overflowing size. */
TuiScreen *tui_screen_create(size_t w, size_t h);
void       tui_screen_free(TuiScreen *s);

size_t tui_screen_width(const TuiScreen *s);
size_t tui_screen_height(const TuiScreen *s);

/* Reset every cell to space / TUI_ATTR_NONE. */
void tui_screen_clear(TuiScreen *s);

/* Write one character at (x,y) with attributes. Out-of-range writes are
 * ignored. Non-printable bytes (< 0x20 or 0x7f) are stored as a space. */
void tui_screen_put(TuiScreen *s, size_t x, size_t y, char ch, uint8_t attr);

/* Read a cell's character (space if out of range). */
char tui_screen_char(const TuiScreen *s, size_t x, size_t y);
/* Read a cell's attribute (TUI_ATTR_NONE if out of range). */
uint8_t tui_screen_attr(const TuiScreen *s, size_t x, size_t y);

/* Plain-text dump: rows joined by '\n' (trailing spaces preserved), a final
 * '\n' after the last row. Returns malloc'd NUL-terminated string (caller
 * frees), NULL on OOM. Attribute-free -- for tests and headless output. */
char *tui_screen_dump(const TuiScreen *s);

/* Render an ANSI byte stream that paints this screen from the top-left (home).
 * Includes SGR attribute runs. If `prev` is non-NULL and the same size, only
 * changed cells are emitted (minimal repaint); pass NULL for a full paint.
 * Returns malloc'd bytes + sets *out_len (caller frees), NULL on OOM. */
char *tui_screen_render(const TuiScreen *s, const TuiScreen *prev, size_t *out_len);

/* Copy `src` into `dst` (must be the same size). Used to keep a "previous
 * frame" for diff rendering. No-op if sizes differ. */
void tui_screen_copy(TuiScreen *dst, const TuiScreen *src);

#ifdef __cplusplus
}
#endif

#endif /* WUBUTUI_SCREEN_H */
