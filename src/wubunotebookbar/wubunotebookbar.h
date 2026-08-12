/* wubunotebookbar.h — notebook bar: sheet-tab strip at the bottom of a
 * spreadsheet. Model of open sheet tabs + the active one. */
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

#endif
