/* draw.h -- dependency-free C11 vector drawing model (wubudraw).
 *
 * A tiny retained-mode 2D scene: rectangles, ellipses, lines, polylines and
 * text. Renders to SVG (W3C SVG 1.1), finalized through wubusvg so output is
 * guaranteed well-formed. This is the "Draw" half of the office Draw+Math UI;
 * the scene can be attached to a wubumodel SHAPE node or embedded in a doc.
 *
 * Self-contained: opaque scene, no globals, no third-party. */
#ifndef WUBUOFFICE_DRAW_H
#define WUBUOFFICE_DRAW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DrawScene DrawScene;

/* Create a scene of pixel size w x h. */
DrawScene *draw_create(int w, int h);
void       draw_destroy(DrawScene *s);

/* Add primitives. Colors are CSS color strings ("#rrggbb", "none", "red"). */
void draw_add_rect(DrawScene *s, double x, double y, double w, double h,
                   const char *fill, const char *stroke);
void draw_add_ellipse(DrawScene *s, double cx, double cy, double rx, double ry,
                      const char *fill, const char *stroke);
/* A straight line from (x1,y1) to (x2,y2) with stroke width `sw`. */
void draw_add_line(DrawScene *s, double x1, double y1, double x2, double y2,
                   const char *stroke, double sw);
/* Polyline through n points (each "x,y"). If closed, an extra segment joins
 * the last point back to the first. */
void draw_add_polyline(DrawScene *s, const double *pts, int n,
                       const char *stroke, double sw, int closed);
/* Text anchored at (x,y) (baseline) in `size` px. */
void draw_add_text(DrawScene *s, double x, double y, const char *text,
                   double size, const char *fill);

/* Render to a NUL-terminated SVG string (caller frees). Returns NULL on OOM.
 * Output is validated through wubusvg. */
char *draw_render_svg(DrawScene *s);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_DRAW_H */
