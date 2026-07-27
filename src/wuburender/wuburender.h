#ifndef WUBURENDER_H
#define WUBURENDER_H
/* wuburender -- shared wubumodel_doc -> RGBA page renderer for WuBuOffice.
 *
 * This is the SINGLE render path for the office document surface. Both the
 * offscreen PNG writer (apps/wubuwordview) and the live SDL window
 * (apps/wubuword) call wurender_render_doc(), so they can never diverge.
 *
 * Rasterizes paragraphs/headings with FreeType, draws red wavy wubuspell
 * squiggles under misspellings, and embeds a native wubuchart bar chart.
 * UTF-8 safe (decodes codepoints via wububase before rasterizing).
 *
 * C11, 0 warnings under -Wall -Wextra -Wpedantic.
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wurender Wurender;

/* Create a renderer (inits FreeType + loads UI fonts). Returns NULL on fail
 * (e.g. no usable font on the system). */
Wurender *wurender_create(void);
void wurender_destroy(Wurender *r);

/* Render doc into a freshly malloc'd RGBA buffer (caller frees with free()).
 * Returns 0 ok. The rgba, w, h outputs are set. The buffer is W*H*4 bytes,
 * top-down, alpha forced to 255. */
/* Forward decl: full type from model.h (included by the implementation). */
typedef struct wubumodel_doc wubumodel_doc;
int wurender_render_doc(Wurender *r, const wubumodel_doc *doc,
                        int W, int H,
                        unsigned char **rgba, int *w, int *h);

/* Build the bundled demo document (shared by the offscreen + live apps).
 * Caller owns the returned doc and must wubumodel_doc_destroy() it. */
wubumodel_doc *wurender_sample_doc(void);

#ifdef __cplusplus
}
#endif
#endif /* WUBURENDER_H */
