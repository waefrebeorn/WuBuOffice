#ifndef WUBUSSHOW_SHOW_H
#define WUBUSSHOW_SHOW_H

#include "../wubuoxml/package.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PresentationML (pptx) builder: a presentation with N slides, each a title +
 * body text box. Assembled into a valid .pptx by wubushow_assemble(). */

typedef struct wubushow_pres wubushow_pres;

wubushow_pres *wubushow_create(void);
void wubushow_free(wubushow_pres *p);

/* Add a slide with a title and a body paragraph. */
int wubushow_slide(wubushow_pres *p, const char *title, const char *body);

/* Assemble .pptx at outpath. Returns 0 on success. */
int wubushow_assemble(wubushow_pres *p, const char *outpath);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSSHOW_SHOW_H */
