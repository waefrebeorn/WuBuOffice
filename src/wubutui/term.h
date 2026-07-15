/* term.h -- wubutui terminal control: raw mode, alt screen, size, I/O.
 *
 * The single module that talks to a real TTY (POSIX termios + ANSI). Everything
 * else in wubutui is pure and testable; this thin layer is what an interactive
 * app links against. Keep it minimal so the pure core stays the bulk of the
 * logic (soul.md: isolate the impure edge).
 */
#ifndef WUBUTUI_TERM_H
#define WUBUTUI_TERM_H

#include "screen.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TuiTerm TuiTerm;

/* Enter raw mode on the controlling terminal (stdin/stdout): disables echo and
 * canonical mode, switches to the alternate screen buffer, hides the cursor.
 * Returns a handle, or NULL if stdout is not a TTY or setup failed. Always pair
 * with tui_term_leave() to restore the terminal. */
TuiTerm *tui_term_enter(void);

/* Restore the original terminal state (cooked mode, main screen, cursor). */
void tui_term_leave(TuiTerm *t);

/* Current terminal size in cells. Falls back to 80x24 if the ioctl fails. */
void tui_term_size(TuiTerm *t, size_t *w, size_t *h);

/* Paint `s` to the terminal, diffing against the previously painted frame for a
 * minimal repaint. The term keeps its own copy of the last frame. */
void tui_term_present(TuiTerm *t, const TuiScreen *s);

/* Read available input bytes (blocking for at least one) into `buf`; returns
 * the count (0 on EOF/interrupt). Caller feeds these to tui_key_decode(). */
size_t tui_term_read(TuiTerm *t, char *buf, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* WUBUTUI_TERM_H */
