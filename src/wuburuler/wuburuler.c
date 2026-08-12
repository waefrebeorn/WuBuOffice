#include "wuburuler.h"

int wuburuler_init(wuburuler *r, double pw, double ph) {
    if (!r || pw <= 0 || ph <= 0) return -1;
    r->page_width = pw; r->page_height = ph;
    r->left = r->right = r->top = r->bottom = 72.0; /* 1 inch default */
    return 0;
}

int wuburuler_set_margins(wuburuler *r, double l, double rgt, double t, double b) {
    if (!r || l < 0 || rgt < 0 || t < 0 || b < 0) return -1;
    if (l + rgt >= r->page_width || t + b >= r->page_height) return -1;
    r->left = l; r->right = rgt; r->top = t; r->bottom = b;
    return 0;
}

int wuburuler_content(wuburuler *r, double *w, double *h) {
    if (!r || !w || !h) return -1;
    *w = r->page_width - r->left - r->right;
    *h = r->page_height - r->top - r->bottom;
    return 0;
}
