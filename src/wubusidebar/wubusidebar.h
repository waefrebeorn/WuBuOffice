/* wubusidebar.h — sidebar panel manager: named, ordered panels. */
#ifndef WUBUSIDEBAR_H
#define WUBUSIDEBAR_H
#include <stddef.h>

typedef struct wubusidebar wubusidebar;

wubusidebar *wubusidebar_create(void);
void wubusidebar_destroy(wubusidebar *s);

/* Register a panel with a title. Returns 0. */
int wubusidebar_add_panel(wubusidebar *s, const char *title);

size_t wubusidebar_count(wubusidebar *s);
const char *wubusidebar_title(wubusidebar *s, size_t i);

/* Active panel index. */
int wubusidebar_set_active(wubusidebar *s, size_t i);
size_t wubusidebar_active(wubusidebar *s);

/* Whether the sidebar is shown. */
int wubusidebar_show(wubusidebar *s, int show);
int wubusidebar_visible(wubusidebar *s);

#endif
