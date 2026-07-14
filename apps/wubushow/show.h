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

/* --- read-back accessors (for round-trip verification and callers that load
 * an existing .pptx). The presentation is opaque; inspect it through these. */
int  wubushow_slide_count(const wubushow_pres *p);
/* Returns 0 and fills out-title/out-body (pointers into internal storage,
 * valid until the presentation is freed) for slide idx (0-based). Non-zero if
 * the index is out of range. */
int  wubushow_slide_get(const wubushow_pres *p, int idx, const char **out_title, const char **out_body);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSSHOW_SHOW_H */
