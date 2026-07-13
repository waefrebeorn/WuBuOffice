#ifndef WUBUEDIT_EDIT_H
#define WUBUEDIT_EDIT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* wubuedit: round-trip re-writer. Read an OOXML document (docx/xlsx/pptx),
 * extract its visible text, and re-emit a fresh WuBuOffice document that
 * preserves that text. This proves the reader+writer loop is lossless for the
 * parts we understand. Returns 0 on success, -1 on error. */
int wubuedit_roundtrip(const char *in_path, const char *out_path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUEDIT_EDIT_H */
