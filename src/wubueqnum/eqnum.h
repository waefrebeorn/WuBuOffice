/* eqnum.h -- sequential equation numbering (DOC-69). Walks a document model,
 * numbers every equation-bearing node (FIELD-kind, used by the math insert) in
 * document order, and stores "(n)" labels keyed by node id. Opaque. */
#ifndef WUBUEQNUM_H
#define WUBUEQNUM_H
#include <stdint.h>

typedef struct EqNum EqNum;

EqNum *eqnum_create(void);
void   eqnum_destroy(EqNum *e);

/* Number all equation nodes in `doc`; returns the count numbered. */
int    eqnum_scan(EqNum *e, const void *doc);

const char *eqnum_label(EqNum *e, uint64_t node_id);  /* "(n)" or NULL */
int    eqnum_count(EqNum *e);

#endif /* WUBUEQNUM_H */
