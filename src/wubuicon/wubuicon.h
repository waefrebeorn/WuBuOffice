/* wubuicon.h — icon library: named vector/bitmap icon registry. */
#ifndef WUBUICON_H
#define WUBUICON_H
#include <stddef.h>

typedef struct wubuicon wubuicon;

wubuicon *wubuicon_create(void);
void wubuicon_destroy(wubuicon *i);

/* Register an icon under a name (SVG path data or bitmap id). Returns 0. */
int wubuicon_add(wubuicon *i, const char *name, const char *data);

/* Look up an icon's data by name, or NULL. */
const char *wubuicon_get(const wubuicon *i, const char *name);

size_t wubuicon_count(const wubuicon *i);
const char *wubuicon_name(const wubuicon *i, size_t idx);

#endif
