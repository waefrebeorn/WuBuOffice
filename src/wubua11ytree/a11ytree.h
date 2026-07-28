/* a11ytree.h -- UI -> accessibility-tree serializer (UXA-40). Walks a document
 * model and emits a flat, line-oriented a11y tree (role + accessible name) that
 * a screen reader / platform bridge can consume. Opaque over the model. */
#ifndef WUBUA11YTREE_H
#define WUBUA11YTREE_H

#include <stdint.h>

/* Build an a11y-tree string for `doc`. Returns a malloc'd, NUL-terminated
 * string (caller frees) where each line is "ROLE: name" (name omitted if none).
 * Returns NULL on error. Roles: SECTION, PARAGRAPH, RUN, IMAGE, LINK, TABLE,
 * LIST, HEADING (section depth encoded as HEADING:n). */
char *a11ytree_build(const void *doc);

/* Count the nodes that will appear in the tree (for UI badges). */
int   a11ytree_count(const void *doc);

#endif /* WUBUA11YTREE_H */
