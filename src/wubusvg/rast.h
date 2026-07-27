/* rast.h -- minimal SVG -> RGBA rasterizer (wubusvg consumption path).
 *
 * Charts (wubuchart), Draw (wubudraw) and Math (wubumath) all emit SVG strings
 * that previously "went nowhere" (RESEARCH_GAPS_100 gap #13). This module is
 * the missing consumer: it rasterizes the primitive subset those engines emit
 * (rect, line, ellipse, polyline, text) onto an RGBA buffer so the office
 * shell can actually display them.
 *
 * Scope is deliberately small and dependency-free: it walks the wubusvg
 * element tree and paints the handful of primitives the office engines use.
 * Text is painted via a caller-supplied callback (kept out of the engine
 * layer so wubusvg has no GUI dependency); pass NULL to skip text. */
#ifndef WUBUSVG_RAST_H
#define WUBUSVG_RAST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Text paint callback: draw UTF-8 `s` at baseline (x,y) in `size` px, color
 * (r,g,b), onto RGBA fb (w*h*4). The Document view supplies wuos_font_draw. */
typedef void (*svg_text_fn)(const char *s, int x, int y, int size,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char *fb, int w, int h);

/* Rasterize an SVG byte stream into a freshly malloc'd RGBA buffer
 * (w*h*4, opaque white background). On success returns 1 and sets *out
 * (caller frees with free()), *w, *h. Returns 0 on parse failure or OOM.
 * `text_fn` may be NULL (text shapes are skipped). */
int svg_rasterize_cb(const char *svg, size_t len,
                     unsigned char **out, int *w, int *h,
                     svg_text_fn text_fn);

/* Convenience: rasterize without text (engine-only use / tests). */
int svg_rasterize(const char *svg, size_t len,
                  unsigned char **out, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSVG_RAST_H */
