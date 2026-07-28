/* heading.h -- semantic heading-level enforcement (UXA-49/50). Walks the
 * document's SECTION nodes (headings) in document order and assigns sequential
 * levels 1,2,3... (no skipping), stored keyed by node id. The outline pane /
 * EPUB exporter reads the enforced levels. Opaque. */
#ifndef WUBUHEADING_H
#define WUBUHEADING_H
#include <stdint.h>

typedef struct Heading Heading;

Heading *heading_create(void);
void     heading_destroy(Heading *h);

/* Enforce sequential levels on `doc`'s SECTION nodes; returns count. */
int      heading_enforce(Heading *h, const void *doc);

/* Enforced level for a section node id (1-based), or 0 if not a heading. */
int      heading_level(Heading *h, uint64_t section_id);

int      heading_count(Heading *h);

#endif /* WUBUHEADING_H */
