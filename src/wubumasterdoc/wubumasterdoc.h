/* wubumasterdoc.h — Master Document: an ordered list of referenced
 * sub-documents (files). A master doc assembles its children in order,
 * optionally with inline text between them. */
#ifndef WUBUMASTERDOC_H
#define WUBUMASTERDOC_H
#include <stddef.h>

typedef struct wubumasterdoc wubumasterdoc;

wubumasterdoc *wubumasterdoc_create(void);
void wubumasterdoc_destroy(wubumasterdoc *m);

/* Append a sub-document reference at the end. Returns 0 on success. */
int wubumasterdoc_add(wubumasterdoc *m, const char *path);

/* Insert at index `at`. Returns 0. */
int wubumasterdoc_insert(wubumasterdoc *m, size_t at, const char *path);

/* Remove the i-th sub-document. Returns 1 if removed, 0 if out of range. */
int wubumasterdoc_remove(wubumasterdoc *m, size_t i);

size_t wubumasterdoc_count(const wubumasterdoc *m);
const char *wubumasterdoc_get(const wubumasterdoc *m, size_t i);

#endif
