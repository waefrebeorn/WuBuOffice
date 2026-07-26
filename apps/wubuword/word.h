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

/* Lists. Begin a list, then add items; end closes it. `kind` is "bullet" or
 * "number" (decimal). Items nest by calling list_begin again (one level deep
 * is sufficient for the common case). Each item is a paragraph carrying
 * numbering properties (w:numPr) referencing the numbering definition that
 * wubuword emits into word/numbering.xml when any list is used. */
void wubuword_list_begin(wubuword_doc *d, const char *kind);
void wubuword_list_item(wubuword_doc *d, const char *text);
void wubuword_list_end(wubuword_doc *d);

/* True if the document used any list (so the caller must include the
 * numbering part). Lets wubuword_assemble emit word/numbering.xml. */
int  wubuword_used_lists(const wubuword_doc *d);

/* Begin/end a table. Call wubuword_cell() between begin and end to fill rows.
 * Each row is started with wubuword_row() (call it before the cells of a row;
 * it closes the previous row if one is open). */
void wubuword_table_begin(wubuword_doc *d);
void wubuword_row(wubuword_doc *d);
void wubuword_cell(wubuword_doc *d, int bold, const char *text);
void wubuword_table_end(wubuword_doc *d);

/* Serialize the document to a heap buffer (WordprocessingML). */
char *wubuword_render(wubuword_doc *d, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUWORD_WORD_H */
