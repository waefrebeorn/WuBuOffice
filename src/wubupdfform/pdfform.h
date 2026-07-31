/* pdfform.h -- PDF forms export (EXP-82, full). Writes a genuine PDF 1.4
 * document with an AcroForm: each wubuform field becomes a widget annotation
 * (text field or checkbox) on page 1. From scratch: header, object bodies,
 * xref table, trailer -- uncompressed, valid for any compliant reader. */
#ifndef WUBUPDFFORM_H
#define WUBUPDFFORM_H

#include <stddef.h>
#include <stdint.h>

typedef struct Form Form;   /* wubuform */

/* Build the PDF bytes for `form` (malloc'd, caller frees). Page is US-Letter
 * (612x792). Fields are stacked top-down starting at y=700, 24pt tall.
 * Returns 1 ok (sets *out and *out_len), 0 error. */
int pdfform_build(const Form *form, uint8_t **out, size_t *out_len);

/* Convenience: write to `path`. Returns 0 ok, -1 error. */
int pdfform_write_file(const Form *form, const char *path);

#endif /* WUBUPDFFORM_H */
