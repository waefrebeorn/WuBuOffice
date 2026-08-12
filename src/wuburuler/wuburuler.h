/* wuburuler.h — page ruler / margin model. */
#ifndef WUBURULER_H
#define WUBURULER_H

typedef struct {
    double left, right, top, bottom;  /* page margins in points */
    double page_width, page_height;   /* page size in points */
} wuburuler;

int wuburuler_init(wuburuler *r, double pw, double ph);
int wuburuler_set_margins(wuburuler *r, double l, double rgt, double t, double b);
/* Content width and height inside margins. Returns 0 and writes out. */
int wuburuler_content(wuburuler *r, double *w, double *h);

#endif
