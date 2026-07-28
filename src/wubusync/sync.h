/* sync.h -- local-first document sync store (COL-92) + shared lock (COL-96).
 *
 * A document is identified by a stable key (e.g. its path hash). Each key maps
 * to a CRDT replica file under a local store directory. `sync_put` writes the
 * replica (last-writer-wins by wall-clock + site tiebreak); `sync_merge` pulls a
 * peer replica and merges it; `sync_get` loads the current replica for editing.
 *
 * The shared lock (COL-96) is a per-doc lockfile carrying the holder pid + site;
 * `sync_lock` succeeds only if no live holder, preventing two editors from
 * editing the same key unsafely. */
#ifndef WUBUSYNC_H
#define WUBUSYNC_H

#include <stddef.h>

typedef struct Sync Sync;

/* Open (creating if needed) the local store at `dir`. Returns NULL on failure. */
Sync *sync_open(const char *dir);
void  sync_close(Sync *s);

/* Persist the serialized replica `blob`/`len` for `key` (LWW). Returns 1 ok. */
int   sync_put(Sync *s, const char *key, const char *blob, size_t len,
               const char *site);
/* Load the replica blob for `key` into a malloc'd buffer (caller frees) + len.
 * Returns 1 if found, 0 if absent. */
int   sync_get(Sync *s, const char *key, char **out_blob, size_t *out_len);
/* Merge a peer replica blob into the stored one (CRDT convergence). Returns the
 * number of items added, or -1 on error. */
int   sync_merge(Sync *s, const char *key, const char *peer_blob, size_t len,
                 const char *site);

/* Shared lock for `key`. `pid` identifies the holder. Returns 1 if acquired
 * (no live holder), 0 if already held by a live process. */
int   sync_lock(Sync *s, const char *key, int pid, const char *site);
/* Release the lock held by `pid` (only the holder can release). Returns 1. */
int   sync_unlock(Sync *s, const char *key, int pid);

#endif /* WUBUSYNC_H */
