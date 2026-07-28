/* gotoline.h -- opaque "Go to line" mini-prompt for the editor.
 *
 * Owns the prompt's open-mode flag and the numeric line buffer (previously
 * inline `goto_mode` + `goto_buf[32]` in the Editor struct). The editor keeps
 * only a GotoLine* handle; it forwards keys while active and renders the bar
 * from gotoline_buf(). The actual document jump is performed by the editor via
 * gotoline_commit(), which calls back into the editor's cursor API so this
 * module stays decoupled from the Doc.
 */
#ifndef WUBUOS_GOTOLINE_H
#define WUBUOS_GOTOLINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GotoLine GotoLine;

GotoLine *gotoline_create(void);
void      gotoline_destroy(GotoLine *g);

/* Open / close the prompt. */
void gotoline_open(GotoLine *g);
void gotoline_close(GotoLine *g);
int  gotoline_active(const GotoLine *g);

/* Feed a key while active. Returns:
 *   0 = prompt handled the key (stays open or closed itself)
 *   1 = user committed (Enter) -> call gotoline_commit(g, jump_cb, ctx)
 *   2 = user cancelled (Esc)   -> prompt closed
 * The editor should call gotoline_commit() when this returns 1. */
int gotoline_key(GotoLine *g, int key);

/* The current buffer text (for rendering the prompt bar). */
const char *gotoline_buf(const GotoLine *g);

/* Parse the buffered number (1-based line). Returns 0 if empty/invalid.
 * The editor resolves the line number to a byte offset and performs the jump,
 * so this module stays decoupled from the Doc. Closes the prompt. */
int gotoline_commit(const GotoLine *g);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOS_GOTOLINE_H */
