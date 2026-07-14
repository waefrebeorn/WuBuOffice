#ifndef WUBUCELL_CELL_H
#define WUBUCELL_CELL_H

#include "../wubuoxml/package.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SpreadsheetML (xlsx) builder: one workbook with N worksheets, cells as
 * inline strings or numbers. Serialized to payloads and assembled into a
 * valid .xlsx by wubucell_assemble(). Supports a full style registry
 * (fonts/fills/borders/number-formats) and embedded charts. */

typedef struct wubucell_book wubucell_book;
struct wubucell_style;   /* defined in style.h */

wubucell_book *wubucell_create(void);
void wubucell_free(wubucell_book *b);

/* Access the shared style registry (so callers can define fonts/fills/etc).
 * The registry is created lazily on first access. */
struct wubucell_style *wubucell_styles(wubucell_book *b);

/* Add a worksheet; returns a 1-based sheet index used by wubucell_cell(). */
int wubucell_sheet(wubucell_book *b, const char *name);

/* Set a cell. `col`/'row' are 1-based. `style` is a cellXfs index from the
 * style registry (0 = default). Pass -1 for default.
 *  - wubucell_cell_s : inline/shared string
 *  - wubucell_cell_n : numeric
 *  - wubucell_cell_f : formula (text is the formula, e.g. "A1+A2"; stored in
 *                       the <f> element; optional cached value `cached`). */
void wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text);
void wubucell_cell_sx(wubucell_book *b, int sheet, int col, int row, const char *text, int style);
void wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num);
void wubucell_cell_nx(wubucell_book *b, int sheet, int col, int row, double num, int style);
void wubucell_cell_f(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached);
void wubucell_cell_fx(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached, int style);

/* Add a bar chart to a sheet referencing a cell range. `title` may be NULL.
 * cats = "Sheet1!A2:A5" style range of category labels, vals = numeric range.
 * Returns a 1-based chart index (used only for bookkeeping). */
int wubucell_chart(wubucell_book *b, int sheet, const char *title,
                   const char *cats, const char *vals);

/* Use a shared string table instead of inline strings (the real Excel
 * default; smaller for repeated text). Off by default. */
void wubucell_use_shared_strings(wubucell_book *b, int enable);

/* --- read-back accessors (for round-trip verification and callers that load
 * an existing .xlsx). A `wubucell_book` returned by wubucell_read exposes its
 * sheets and cells through these opaque-but-inspectable accessors; no internal
 * header is required. */
typedef enum { WUBUCELL_STR, WUBUCELL_NUM, WUBUCELL_FORM } wubucell_ckind;

int  wubucell_sheet_count(const wubucell_book *b);
/* Cell lookup by 1-based sheet/col/row. Returns 0 if a cell exists there
 * (filling *kind, *text, *num, *cached), non-zero if the slot is empty.
 * `text` points at internal storage and is valid until the book is freed. */
int  wubucell_get(const wubucell_book *b, int sheet, int col, int row,
                  wubucell_ckind *kind, const char **text, double *num, double *cached);

/* Assemble .xlsx at outpath. Returns 0 on success. */
int wubucell_assemble(wubucell_book *b, const char *outpath);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_CELL_H */
