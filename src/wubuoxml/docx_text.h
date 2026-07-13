#ifndef WUBUOXML_DOCX_TEXT_H
#define WUBUOXML_DOCX_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plain-text extraction from a WordprocessingML part (word/document.xml).
 * Collects the content of every <w:t> element, decoding the common XML
 * entities (&amp; &lt; &gt;). The result is heap-allocated, NUL-terminated,
 * and owned by the caller (free it). Returns 0 on success, -1 on alloc error. */
int wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_DOCX_TEXT_H */
