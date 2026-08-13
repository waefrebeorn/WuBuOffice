/* wubunotebookbar.h — notebook bar: sheet-tab strip at the bottom of a
 * spreadsheet. Model of open sheet tabs + the active one, plus the geometry a
 * renderer needs to draw + hit-test the strip. */
#ifndef WUBUNOTEBOOKBAR_H
#define WUBUNOTEBOOKBAR_H
#include <stddef.h>

typedef struct wubunotebookbar wubunotebookbar;

wubunotebookbar *wubunotebookbar_create(void);
void wubunotebookbar_destroy(wubunotebookbar *n);

int wubunotebookbar_add(wubunotebookbar *n, const char *name);
size_t wubunotebookbar_count(const wubunotebookbar *n);
const char *wubunotebookbar_name(const wubunotebookbar *n, size_t i);

int wubunotebookbar_set_active(wubunotebookbar *n, size_t i);
size_t wubunotebookbar_active(const wubunotebookbar *n);

/* Compute the screen rect of tab `i` in a strip starting at `x0` (left edge),
 * baseline `y` (top), with uniform `tab_w` width and `tab_h` height.
 * Writes x,y,w,h. Returns 0, or -1 on bad index. Used by the renderer to draw
 * and by hit-testing to map a click to a tab. */
int wubunotebookbar_tab_rect(const wubunotebookbar *n, size_t i,
                             double x0, double y, double tab_w, double tab_h,
                             double *x, double *yy, double *w, double *h);

#endif
