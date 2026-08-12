/* wubumasterslide.h — master slide / slide layout model. */
#ifndef WUBUMASTERSLIDE_H
#define WUBUMASTERSLIDE_H
#include <stddef.h>

typedef struct wubumasterslide wubumasterslide;

wubumasterslide *wubumasterslide_create(void);
void wubumasterslide_destroy(wubumasterslide *m);

/* Set the master's background color (RRGGBB) and default font size (pt). */
int wubumasterslide_set_theme(wubumasterslide *m, const char *bg, double fontsize);
const char *wubumasterslide_background(const wubumasterslide *m);
double wubumasterslide_fontsize(const wubumasterslide *m);

#endif
