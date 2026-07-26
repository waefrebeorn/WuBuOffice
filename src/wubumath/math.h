/* math.h -- dependency-free C11 math expression renderer (wubumath).
 *
 * Renders a math expression (a pragmatic LaTeX-ish subset: \frac{a}{b},
 * superscripts x^2, subscripts x_0, parentheses grouping, and plain
 * identifiers/numbers/operators) to SVG. Layout is done with a box model
 * (width/height/baseline); glyph metrics are estimated from font size (no
 * font dependency). Output is finalized through wubusvg.
 *
 * Self-contained: opaque parse/layout, no globals, no third-party. */
#ifndef WUBUOFFICE_MATH_H
#define WUBUOFFICE_MATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Render `expr` to a NUL-terminated SVG string (caller frees). Returns NULL
 * on OOM or a parse error. The SVG is validated through wubusvg. */
char *math_render_svg(const char *expr);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_MATH_H */
