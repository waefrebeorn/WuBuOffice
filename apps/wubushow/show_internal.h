/* WuBuOffice -- apps/wubushow/show_internal
 * Internal definitions for the PresentationML (pptx) builder. The wubushow_pres
 * struct is opaque to callers (see show.h); only show_*.c include this header.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#ifndef WUBUSSHOW_SHOW_INTERNAL_H
#define WUBUSSHOW_SHOW_INTERNAL_H

#include "show.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *title; char *body; } slide_t;

struct wubushow_pres {
    slide_t *slides; size_t n, cap;
};

/* model.c */
void show_xml_escape(FILE *m, const char *t);
char *show_render_slide(const slide_t *s, int idx);
void show_render_body(FILE *m, const char *body);

#endif /* WUBUSSHOW_SHOW_INTERNAL_H */
