#ifndef WUBUWORD_WORD_H
#define WUBUWORD_WORD_H

#include "../wubuoxml/package.h"
#include "../wubuxml/xml.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Richer WordprocessingML builder: paragraphs (optionally bold / heading),
 * and tables. Returns payload bytes for word/document.xml. Caller frees. */
typedef struct wubuword_doc wubuword_doc;

wubuword_doc *wubuword_create(void);
void wubuword_free(wubuword_doc *d);

/* Add a paragraph. `style` may be "Heading1".."Heading3", "Title", or NULL.
 * `bold` nonzero makes the run bold. */
void wubuword_para(wubuword_doc *d, const char *style, int bold, const char *text);

/* Begin/end a table. Call wubuword_cell() between begin and end to fill rows. */
void wubuword_table_begin(wubuword_doc *d);
void wubuword_cell(wubuword_doc *d, int bold, const char *text);
void wubuword_table_end(wubuword_doc *d);

/* Serialize the document to a heap buffer (WordprocessingML). */
char *wubuword_render(wubuword_doc *d, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUWORD_WORD_H */
