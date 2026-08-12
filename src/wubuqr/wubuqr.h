/* wubuqr.h — QR code generation for documents (wraps the wubuocr QR codec).
 * Emits a QR as an ASCII art matrix (0/1 rows) usable for document export. */
#ifndef WUBUQR_H
#define WUBUQR_H
#include <stddef.h>

/* Encode `text` to a QR and render as a malloc'd ASCII matrix: each row is
 * `size` characters of '#'/'.', rows separated by '\n', NUL-terminated.
 * Returns malloc'd string or NULL if text too long. Caller frees. */
char *wubuqr_render_ascii(const char *text, int *out_size);

#endif
