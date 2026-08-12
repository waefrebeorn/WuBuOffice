/* wubugallery.h — gallery: named collection of reusable items (images,
 * shapes, clip-art). */
#ifndef WUBUGALLERY_H
#define WUBUGALLERY_H
#include <stddef.h>

typedef struct wubugallery wubugallery;

typedef struct {
    char **items;  /* item ids/paths */
    size_t n;
} wubugallery_col;

wubugallery *wubugallery_create(void);
void wubugallery_destroy(wubugallery *g);

/* Create or get a named gallery collection. */
wubugallery_col *wubugallery_get(wubugallery *g, const char *name);

/* Add an item to a named gallery (creates it if absent). Returns 0. */
int wubugallery_add_item(wubugallery *g, const char *name, const char *item);

size_t wubugallery_count(wubugallery *g);
const char *wubugallery_name(wubugallery *g, size_t i);

#endif
