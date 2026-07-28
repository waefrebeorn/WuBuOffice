/* history.h -- opaque version-history store for a document.
 *
 * Callers hand in opaque serialized snapshots (any byte blob, e.g. a CRDT
 * serialize() buffer) tagged with a label + author. The store keeps every
 * version, can list them newest-first, restore any version's blob, and produce
 * a unified line-diff between two versions for the UI. No actual document
 * format is assumed here. */
#ifndef WUBUHISTORY_H
#define WUBUHISTORY_H

#include <stddef.h>

typedef struct History History;

History *history_create(void);
void     history_destroy(History *h);

/* Record a new version. `blob`/`len` is the serialized snapshot (copied).
 * `label`/`author` may be NULL. Returns the new version id (1-based, increasing)
 * or 0 on failure. */
int  history_commit(History *h, const char *blob, size_t len,
                    const char *label, const char *author);

/* Number of stored versions. */
int  history_count(const History *h);
/* Version id at list index i (0 = oldest). 0 if out of range. */
int  history_id_at(const History *h, int i);
/* Snapshot blob + length for a version id (NULL/invalid -> NULL, *out_len=0).
 * Do NOT free the returned pointer (owned by the store). */
const char *history_blob(const History *h, int id, size_t *out_len);
/* Author/label for a version id (NULL if none). Do NOT free. */
const char *history_author(const History *h, int id);
const char *history_label(const History *h, int id);

/* Produce a unified line-diff from version `a` to version `b` (both ids).
 * Returns a malloc'd, NUL-terminated string (caller frees) with lines prefixed
 * '-' (removed) and '+' (added), or NULL on error / equal. */
char *history_diff(History *h, int a, int b);

#endif /* WUBUHISTORY_H */
