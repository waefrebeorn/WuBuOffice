/* palette.h -- UI-29: command palette (Ctrl+K). Pure fuzzy-filter/select
 * logic over a registered command list; the shell draws the result and
 * feeds keys. Headless-testable. Opaque, C11. */
#ifndef WUOS_PALETTE_H
#define WUOS_PALETTE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Palette Palette;

Palette *palette_create(void);
void     palette_destroy(Palette *p);

/* Register a command: display label + an integer command id the shell maps
 * back to an action. Returns 0 on success. */
int palette_add(Palette *p, const char *label, int cmd_id);

/* Open/close/toggle; open resets the query + selection. */
void palette_open(Palette *p);
void palette_close(Palette *p);
int  palette_is_open(const Palette *p);

/* Edit the query: append a character / backspace. Re-filters. */
void palette_input(Palette *p, char c);
void palette_backspace(Palette *p);
const char *palette_query(const Palette *p);

/* Move the selection down/up (wraps). */
void palette_next(Palette *p);
void palette_prev(Palette *p);

/* Filtered results (subsequence match, case-insensitive). */
int         palette_result_count(const Palette *p);
const char *palette_result_label(const Palette *p, int i);
int         palette_result_id(const Palette *p, int i);
int         palette_selected(const Palette *p);

/* Confirm: returns the selected command id (or -1) and closes. */
int palette_confirm(Palette *p);

#ifdef __cplusplus
}
#endif
#endif /* WUOS_PALETTE_H */
