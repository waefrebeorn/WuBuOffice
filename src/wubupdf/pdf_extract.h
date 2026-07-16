/* pdf_extract.h -- clean-room PDF text extraction (read side).
 *
 * WuBuOffice already WRITES PDFs (apps/wubupdf); this module is the missing
 * READ side so a dropped/typed PDF is "interpreted as a document" (the user's
 * drag/drop requirement). No third-party libs: streams compressed with
 * FlateDecode are inflated with the in-tree wubuzip DEFLATE engine, and text
 * is pulled straight from the content-stream operators ( ... ) Tj / [ ... ] TJ.
 *
 * English/Latin-first by design: bytes are decoded as WinAnsi (CP1252) with
 * identity for the Latin-1 range, so accented European text and common
 * typographic quotes/dashes come through; other planes fall back to '?'.
 */
#ifndef WUBUPDF_EXTRACT_H
#define WUBUPDF_EXTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Extract the visible text of a PDF into a malloc'd NUL-terminated UTF-8
 * string (caller frees). Returns NULL on a non-PDF or empty input. The text
 * is block/line structured: a newline is emitted at each text-positioning
 * operator (T*, Td, TD, ET) so paragraphs survive. */
char *pdf_extract_text(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUPDF_EXTRACT_H */
