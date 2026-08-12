/* wubuhyperlink.h — document hyperlink side-table keyed by node id.
 * Maps a stable node id -> {target, display text, anchor}. */
#ifndef WUBUHYPERLINK_H
#define WUBUHYPERLINK_H
#include <stddef.h>
#include <stdint.h>

typedef struct wubuhyperlink wubuhyperlink;

typedef struct {
    char *target;   /* URL or #anchor or file path */
    char *text;     /* display text (may be NULL = use node text) */
    char *anchor;   /* optional fragment target inside doc (may be NULL) */
} wubuhyperlink_entry;

wubuhyperlink *wubuhyperlink_create(void);
void wubuhyperlink_destroy(wubuhyperlink *h);

/* Attach a hyperlink to node id. Returns 0 on success, -1 on OOM. */
int wubuhyperlink_set(wubuhyperlink *h, uint64_t node_id,
                      const char *target, const char *text, const char *anchor);

/* Get the entry for node id, or NULL. The returned entry is owned by h. */
const wubuhyperlink_entry *wubuhyperlink_get(const wubuhyperlink *h, uint64_t node_id);

/* Remove a link; returns 1 if removed, 0 if absent. */
int wubuhyperlink_remove(wubuhyperlink *h, uint64_t node_id);

size_t wubuhyperlink_count(const wubuhyperlink *h);

#endif
