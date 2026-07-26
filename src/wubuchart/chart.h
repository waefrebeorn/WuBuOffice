/* chart.h -- dependency-free C11 chart renderer (bar / line / pie / scatter).
 *
 * Emits SVG (W3C SVG 1.1) from numeric series data. Output is finalized
 * through wubusvg (parse -> regurgitate) so charts share the suite's SVG
 * document pipeline and are guaranteed well-formed. Charts can carry an
 * optional formula-annotated subtitle via wubuformula.
 *
 * Self-contained: opaque struct, no globals, no third-party. */
#ifndef WUBUOFFICE_CHART_H
#define WUBUOFFICE_CHART_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CHART_BAR = 0, CHART_LINE, CHART_PIE, CHART_SCATTER } ChartType;

typedef struct Chart Chart;

/* Create a chart (title may be NULL). */
Chart *chart_create(const char *title);
void    chart_free(Chart *c);

/* Pixel size (default 640x400). */
void chart_set_size(Chart *c, int w, int h);
/* Chart kind (default CHART_BAR). */
void chart_set_type(Chart *c, ChartType t);

/* Optional formula-evaluated subtitle. `formula` is an Excel-style expression
 * WITHOUT the leading '='; evaluated lazily at render time. May be NULL. */
void chart_set_subtitle_formula(Chart *c, const char *formula);

/* Add a data series. `y` is an array of n values; `labels` (may be NULL) is
 * an array of n category labels (used for the x-axis / pie legend). `name`
 * is the series legend label (may be NULL). The arrays are copied. */
void chart_add_series(Chart *c, const char *name,
                      const double *y, int n, const char *const *labels);

/* Render to a NUL-terminated SVG string (caller frees). Returns NULL on OOM
 * or if no series was added. The SVG is validated through wubusvg. */
char *chart_render_svg(Chart *c);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_CHART_H */
