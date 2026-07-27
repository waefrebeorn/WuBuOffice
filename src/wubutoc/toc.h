/* toc.h -- DOC-54: table-of-contents generator.
 * Walks the model for heading paragraphs (style prop "heading" = "1".."6")
 * and resolves each heading's page number through the central wubulayout
 * pipeline (matching the heading's RUN nodes against laid-out runs). A thin
 * consumer of model + layout — no rendering here; views draw the entries.
 * Opaque, C11, no third-party deps. */
#ifndef WUBUTOC_TOC_H
#define WUBUTOC_TOC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Toc Toc;

/* Build a TOC from a model tree. `layout` is an optional wubulayout_doc*:
 * when given, entries carry 1-based page numbers; when NULL, pages are 0.
 * `root` NULL means walk from the doc root. */
Toc *toc_build(void *model_doc, void *root, void *layout);

void toc_free(Toc *t);

int         toc_count(const Toc *t);
const char *toc_title(const Toc *t, int i);   /* borrowed, valid until free */
int         toc_level(const Toc *t, int i);   /* 1..6 */
int         toc_page(const Toc *t, int i);    /* 1-based; 0 = unknown */
void       *toc_node(const Toc *t, int i);    /* the heading paragraph node */

/* Render the TOC as plain indented text ("  Title .... p3\n"), for the
 * Document view side pane and for export. Malloc'd; caller frees. */
char *toc_text(const Toc *t);

#ifdef __cplusplus
}
#endif
#endif /* WUBUTOC_TOC_H */
