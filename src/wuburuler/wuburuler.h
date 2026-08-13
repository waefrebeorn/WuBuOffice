/* wuburuler.h — page ruler / margin model + pixel-geometry resolve. */
#ifndef WUBURULER_H
#define WUBURULER_H
#include <stddef.h>

typedef struct {
    double left, right, top, bottom;  /* page margins in points */
    double page_width, page_height;   /* page size in points */
} wuburuler;

int wuburuler_init(wuburuler *r, double pw, double ph);
int wuburuler_set_margins(wuburuler *r, double l, double rgt, double t, double b);
/* Content width and height inside margins. Returns 0 and writes out. */
int wuburuler_content(wuburuler *r, double *w, double *h);

/* Resolve the content rectangle in PIXELS for a renderer. `dpi` converts points
 * (72pt/inch) to pixels. Writes x,y (top-left of content) and w,h. Returns 0. */
int wuburuler_content_rect(const wuburuler *r, double dpi,
                           double *x, double *y, double *w, double *h);

#endif
