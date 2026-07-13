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

/* Set a cell. `col`/'row' are 1-based. `text` != NULL writes an inline string;
 * otherwise `num' writes a numeric cell. */
void wubucell_cell_s(wubucell_book *b, int sheet, int col, int row, const char *text);
void wubucell_cell_n(wubucell_book *b, int sheet, int col, int row, double num);

/* Assemble .xlsx at outpath. Returns 0 on success. */
int wubucell_assemble(wubucell_book *b, const char *outpath);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_CELL_H */
