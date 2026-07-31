/* xps.h -- minimal XPS (OpenXPS) writer (EXP-83). XPS is a ZIP (stored,
 * uncompressed) of a [Content_Types].xml plus one FixedPage XML per page. This
 * implements the store-only ZIP container + the FixedPage XML for a single
 * page with the given width/height and UTF-8 text runs. Opaque over buffers. */
#ifndef WUBUXPS_H
#define WUBUXPS_H

#include <stddef.h>
#include <stdint.h>

/* Build an XPS document (malloc'd ZIP bytes, caller frees) for one page of
 * `W`x`H` (in 1/96 inch units) containing `text`. Returns 1 on success and
 * sets *out and *out_len; 0 on error. */
int xps_build(const char *text, int W, int H,
              uint8_t **out, size_t *out_len);

/* Convenience: write the XPS document to `path`. Returns 0 on success. */
int xps_write_file(const char *path, const char *text, int W, int H);

#endif /* WUBUXPS_H */
