#ifndef WUBUCELL_STYLE_H
#define WUBUCELL_STYLE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A registry of spreadsheet styles (fonts / fills / borders / number formats
 * / cellXfs), built once and serialized into xl/styles.xml. Cells reference a
 * style by the 0-based index returned by wubucell_style_*. Self-contained:
 * owns all its own string state; the worksheet writer just emits s="<idx>". */

typedef struct wubucell_style wubucell_style;

wubucell_style *wubucell_style_create(void);
void wubucell_style_free(wubucell_style *s);

/* Define a font. `name` is the typeface (e.g. "Calibri"); `size` in points;
 * `color` is an ARGB hex string (e.g. "FF0070C0") or NULL for default.
 * Returns the 0-based font index. */
int wubucell_style_font(wubucell_style *s, const char *name, double size,
                        int bold, int italic, const char *color);

/* Define a solid fill. `color` is an ARGB hex string (e.g. "FFD9E1F2") or NULL
 * for none. Returns the 0-based fill index. */
int wubucell_style_fill(wubucell_style *s, const char *color);

/* Define a border (all four edges). `style` is an ST_BorderPr style string
 * ("thin", "medium", "thick", "none", ...) and `color` an ARGB hex or NULL.
 * Returns the 0-based border index. */
int wubucell_style_border(wubucell_style *s, const char *style, const char *color);

/* Define a custom number format. `code` is the format string (e.g.
 * "0.00" or "#,##0"). Returns the builtin-or-custom numFmtId (>= 164). */
int wubucell_style_numfmt(wubucell_style *s, const char *code);

/* Compose a cell style (xf) from the above indices. `align` is a horizontal
 * alignment string ("left"/"center"/"right"/NULL) or NULL. Returns the 0-based
 * cellXfs index used as the cell's s="<idx>" attribute. */
int wubucell_style_cell(wubucell_style *s, int font, int fill, int border, int numfmt,
                        const char *align);

/* Serialize the whole style sheet to a heap buffer (caller frees). */
char *wubucell_style_render(const wubucell_style *s, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_STYLE_H */
