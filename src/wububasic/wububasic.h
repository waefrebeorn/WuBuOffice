/* wububasic.h — minimal BASIC interpreter (macros / scripting engine).
 * Subset: LET, PRINT, INPUT, IF..THEN..ELSE, FOR..NEXT, GOTO, GOSUB..RETURN,
 * arithmetic + string ops, arrays. Sufficient for document macros. */
#ifndef WUBUBASIC_H
#define WUBUBASIC_H
#include <stddef.h>

typedef struct wububasic wububasic;

wububasic *wububasic_create(void);
void wububasic_destroy(wububasic *b);

/* Load a program (newline-separated numbered or unnumbered lines). Returns 0. */
int wububasic_load(wububasic *b, const char *program);

/* Set an input-provider callback (for INPUT). default reads stdin. */
typedef int (*wububasic_input_fn)(char *buf, size_t cap, void *ud);
void wububasic_set_input(wububasic *b, wububasic_input_fn fn, void *ud);

/* Set an output-consumer callback (for PRINT). default writes to stdout. */
typedef void (*wububasic_output_fn)(const char *s, void *ud);
void wububasic_set_output(wububasic *b, wububasic_output_fn fn, void *ud);

/* Run the program from the start. Returns 0 on success, non-zero on a runtime
 * error (with a message via wububasic_error). */
int wububasic_run(wububasic *b);
const char *wububasic_error(const wububasic *b);

/* Set a variable (e.g. "x", "name$"). Returns 0. */
int wububasic_set_var(wububasic *b, const char *name, const char *value);
const char *wububasic_get_var(const wububasic *b, const char *name);

#endif
