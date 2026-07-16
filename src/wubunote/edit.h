/* edit.h -- wubunote: a pure, self-contained text-buffer model.
 *
 * The editing core for our Notepad++-class terminal editor. It holds the document
 * as an array of NUL-terminated lines (line 0 is the first) plus a cursor
 * (row, col). ALL mutations are pure C11, allocation-checked, and unit-
 * testable WITHOUT a TTY -- the TUI layer (apps/wubunote) only renders
 * this model and forwards key/mouse events to it.
 *
 * Feature set mirrored from Notepad++/Windows Notepad that actually matter in a
 * terminal: line-based editing, insert/delete, goto-line, find-next, line
 * numbers, and a dirty flag. No undo history here (a separate module can wrap
 * it); keep this small and correct.
 */
#ifndef WUBUEDIT_EDIT_H
#define WUBUEDIT_EDIT_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EditBuf EditBuf;

/* Create an empty buffer. Returns NULL on OOM. */
EditBuf *edit_create(void);
/* Free a buffer (and all its lines). Safe to call with NULL. */
void edit_free(EditBuf *b);

/* Load `text` (NUL-terminated, '\n'-separated) into the buffer, replacing
 * any current content. Returns 0 on success, -1 on OOM. An empty string
 * yields exactly one (empty) line. The dirty flag is cleared. */
int edit_load(EditBuf *b, const char *text);
/* Serialize the buffer back to a single NUL-terminated string (caller frees).
 * Returns NULL on OOM. The trailing line has no implicit newline. */
char *edit_serialize(const EditBuf *b);

size_t edit_line_count(const EditBuf *b);
const char *edit_line(const EditBuf *b, size_t row);   /* never NULL ("" for empty) */
int    edit_dirty(const EditBuf *b);                  /* 1 if modified since load */

/* Cursor (row,col) -- col is a character offset within the line, clamped. */
size_t edit_cursor_row(const EditBuf *b);
size_t edit_cursor_col(const EditBuf *b);
void    edit_cursor_set(EditBuf *b, size_t row, size_t col);
void    edit_cursor_home(EditBuf *b);     /* col = 0 */
void    edit_cursor_end(EditBuf *b);      /* col = line length */

/* --- editing primitives (each sets dirty on change) --- */
void edit_put_char(EditBuf *b, char c);     /* insert c at cursor */
void edit_new_line(EditBuf *b);               /* split line at cursor */
void edit_backspace(EditBuf *b);            /* delete char before cursor (or join) */
void edit_delete(EditBuf *b);                /* delete char at cursor (or join) */
void edit_tab(EditBuf *b);                  /* insert a TAB (rendered as indent) */
void edit_arrow_left(EditBuf *b);
void edit_arrow_right(EditBuf *b);
void edit_arrow_up(EditBuf *b);
void edit_arrow_down(EditBuf *b);

/* Goto line N (1-based). Clamps to [1, line_count]. Updates cursor. */
void edit_goto_line(EditBuf *b, size_t one_based);

/* Find `needle` (case-sensitive) starting AFTER the cursor; wraps to the top
 * if not found below. On success moves the cursor there and returns 1; on no
 * match returns 0 and leaves the cursor unchanged. Returns 0 on NULL needle. */
int edit_find_next(EditBuf *b, const char *needle);

#ifdef __cplusplus
}
#endif

#endif /* WUBUEDIT_EDIT_H */
