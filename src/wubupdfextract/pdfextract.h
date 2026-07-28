/* pdfextract.h -- PDF text import/extract (EXP-91). Wraps the clean-room
 * pdf_extract_text to pull text out of a PDF byte buffer or file. */
#ifndef WUBUPDFEXTRACT_H
#define WUBUPDFEXTRACT_H

#include <stddef.h>
#include <stdint.h>

/* Extract text from PDF bytes. Returns a malloc'd, NUL-terminated string
 * (caller frees), or NULL on error / empty. */
char *pdfextract_bytes(const uint8_t *data, size_t len);

/* Extract text from a PDF file path. Returns malloc'd string or NULL. */
char *pdfextract_file(const char *path);

#endif /* WUBUPDFEXTRACT_H */
