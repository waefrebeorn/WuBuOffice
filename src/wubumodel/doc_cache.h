#ifndef WUBUMODEL_DOC_CACHE_H
#define WUBUMODEL_DOC_CACHE_H

/* Opaque, process-wide cache of parsed documents (ws07#1342: re-open
 * is instant). Keyed by (path, file mtime, file size) so a file that
 * changed on disk is never served stale. The cache OWNS any doc it
 * stores: the caller must not destroy a doc it has put.
 *
 * Not part of the public model API; apps link it directly. */

#include "model.h"
#include <stddef.h>

typedef struct doc_cache doc_cache;

/* Create an empty cache (thread-safe: internal mutex). */
doc_cache *doc_cache_create(void);

/* Destroy the cache and every doc it still owns. */
void doc_cache_destroy(doc_cache *c);

/* Return the cached doc for `path` if an entry exists AND its stored
 * (mtime, size) still match the file on disk. Otherwise NULL — the
 * caller must then load the file and doc_cache_put() it. */
wubumodel_doc *doc_cache_get(doc_cache *c, const char *path);

/* Store/refresh `doc` under `path` keyed by (mtime_sec, size). On
 * success the cache takes OWNERSHIP of `doc` (caller must not
 * destroy it). Returns 0 ok, -1 on alloc failure (doc still owned
 * by caller). */
int doc_cache_put(doc_cache *c, const char *path,
                 long mtime_sec, long long size, wubumodel_doc *doc);

/* Drop any entry for `path` (e.g. after a save mutates the file). */
void doc_cache_invalidate(doc_cache *c, const char *path);

#endif /* WUBUMODEL_DOC_CACHE_H */
