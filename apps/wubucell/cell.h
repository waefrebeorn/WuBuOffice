#ifndef WUBUCELL_CELL_H
#define WUBUCELL_CELL_H

#include "../wubuoxml/package.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SpreadsheetML (xlsx) builder: one workbook with N worksheets, cells as
 * inline strings or numbers. Serialized to payloads and assembled into a
 * valid .xlsx by wubucell_assemble(). */

typedef struct wubucell_book wubucell_book;

wubucell_book *wubucell_create(void);
void wubucell_free(wubucell_book *b);

/* Add a worksheet; returns a 1-based sheet index used by wubucell_cell(). */
int wubucell_sheet(wubucell_book *b, const char *name);

/* Set a cell. `col`/'row' are 1-based.
 *  - wubucell_cell_s : inline string
 *  - wubucell_cell_n : numeric
 *  - wubucell_cell_f : formula (text is the formula, e.g. "A1+A2"; stored in
 *                       the <f> element; optional cached value `cached`). */
void wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text);
void wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num);
void wubucell_cell_f(wubucell_book *b, int sheet, int col, int row, const char *formula, double cached);

/* Use a shared string table instead of inline strings (the real Excel
 * default; smaller for repeated text). Off by default. */
void wubucell_use_shared_strings(wubucell_book *b, int enable);

/* Assemble .xlsx at outpath. Returns 0 on success. */
int wubucell_assemble(wubucell_book *b, const char *outpath);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_CELL_H */
