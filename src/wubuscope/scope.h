/* scope.h -- table header-row scope markup (UXA-49): marks a table's first row
 * as a header with scope "col" (or "row"). Stored as a side-table keyed by the
 * table node id; the EPUB/HTML exporter reads it. Opaque. */
#ifndef WUBUSCOPE_H
#define WUBUSCOPE_H
#include <stdint.h>

typedef struct ScopeMap ScopeMap;

ScopeMap *scope_create(void);
void      scope_destroy(ScopeMap *m);

/* Mark table `table_id`'s header row with scope ("col" | "row" | "none"). */
int       scope_set(ScopeMap *m, uint64_t table_id, const char *scope); /* 1 ok */
const char *scope_get(const ScopeMap *m, uint64_t table_id);           /* NULL */
int       scope_count(const ScopeMap *m);
uint64_t  scope_id_at(const ScopeMap *m, int i);

#endif /* WUBUSCOPE_H */
