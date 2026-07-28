/* crdt.h -- node-sequence CRDT (op-based, LWW-merge) for WuBuOffice documents.
 *
 * A CRDT replica holds an ordered set of items, each with a unique id, a
 * Lamport clock, a payload string, and a tombstone flag. Concurrent inserts
 * are ordered deterministically by (lamport, id); deletes are tombstones that
 * lose to a concurrent insert at the same clock. Replicas converge by union.
 *
 * Opaque: callers never reach into the item array. */
#ifndef WUBUCRDT_H
#define WUBUCRDT_H

#include <stddef.h>

typedef struct Crdt Crdt;

/* Create an empty replica with the given site id (any non-empty string, used
 * only to break Lamport ties deterministically). */
Crdt *crdt_create(const char *site);
void  crdt_destroy(Crdt *c);

/* Insert `value` at logical position `pos` (0..count). Returns the new item's
 * id (valid until the next crdt call on this replica -- copy it), or NULL. */
const char *crdt_insert(Crdt *c, int pos, const char *value);
/* Delete the item at logical position `pos` (0..count-1). Returns 1 if a live
 * item was tombstoned, 0 if pos was out of range. */
int   crdt_delete(Crdt *c, int pos);
/* Move the live item at `from` to before position `to`. Returns 1 on success. */
int   crdt_move(Crdt *c, int from, int to);

/* Number of live (non-tombstoned) items. */
int   crdt_count(const Crdt *c);
/* Live item value at logical position `pos` (0..count-1). Do NOT free. */
const char *crdt_get(const Crdt *c, int pos);

/* Merge remote replica `r` into `c`. After this, both replicas carry the
 * union of all items (convergence is a property of the merge, not of equal
 * clocks on the two sites). Returns the number of items newly added. */
int   crdt_merge(Crdt *dst, const Crdt *src);

/* Serialize the full replica (all items, including tombstones+clocks) to a
 * malloc'd buffer the caller must free, or NULL on error. `out_len` receives
 * the byte length. Format: "id lamport tomb value\n" lines. */
char *crdt_serialize(const Crdt *c, size_t *out_len);
/* Load a serialized replica (replacing `c`'s contents). Returns 1 on success. */
int   crdt_deserialize(Crdt *c, const char *buf, size_t len);

#endif /* WUBUCRDT_H */
