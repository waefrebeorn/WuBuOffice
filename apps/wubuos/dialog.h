/* dialog.h -- opaque modal text-input dialog (headless state machine).
 *
 * Owns: title, prompt, the editable text buffer, and the open/confirm/cancel
 * state. The host (shell) draws it and feeds keys; this module is pure C with no
 * SDL dependency so it can be unit-tested headless. Keys are passed already
 * normalized by the host:
 *   - printable ASCII (>=32)  -> append the character
 *   - 8   (Backspace)         -> delete last char
 *   - 13/10 (Enter/Return)    -> confirm (active=0, confirmed=1)
 *   - 27  (Esc)               -> cancel  (active=0, confirmed=0)
 */
#ifndef WUOS_DIALOG_H
#define WUOS_DIALOG_H

#include <stddef.h>

typedef struct Dialog Dialog;

Dialog *dialog_create(void);
void    dialog_destroy(Dialog *d);

/* Open (or re-open) the dialog. `def` may be NULL for an empty buffer. */
void dialog_open(Dialog *d, const char *title, const char *prompt, const char *def);

/* True while the dialog is awaiting input. */
int  dialog_active(const Dialog *d);

/* Feed a normalized key. Returns:
 *   1 if the dialog just confirmed (text available via dialog_text),
 *   2 if it just cancelled,
 *   0 otherwise (still open / key consumed). */
int  dialog_key(Dialog *d, int key, const char *ch);

/* Current editable text (valid until the next dialog_open). Do NOT free. */
const char *dialog_text(const Dialog *d);

/* Dialog title / prompt labels (valid until the next dialog_open). */
const char *dialog_title(const Dialog *d);
const char *dialog_prompt(const Dialog *d);

/* 1 if the last close was a confirm, 0 if cancel/never-confirmed. */
int  dialog_confirmed(const Dialog *d);

#endif /* WUOS_DIALOG_H */
