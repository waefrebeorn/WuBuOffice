/* cell_csv.h -- CSV / TSV import & export for the wubucell spreadsheet model.
 *
 * RFC 4180 compliant: fields containing the delimiter, a double-quote, or a
 * newline are quoted; embedded quotes are doubled. Import auto-detects numeric
 * fields (stored as numbers) vs text. Only the first sheet is exported (CSV is
 * single-table); import fills sheet 1.
 *
 * Clean-room, from-scratch (SLERM): no third-party CSV code. */

#ifndef WUBUCELL_CELL_CSV_H
#define WUBUCELL_CELL_CSV_H

#include "cell.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read a delimited text file into a fresh book (single sheet named "Sheet1").
 * `delim` is the field separator (',' for CSV, '\t' for TSV). Returns 0 on
 * success; *out receives a book the caller must wubucell_free(). */
int wubucell_read_csv(const char *path, char delim, wubucell_book **out);

/* Write sheet `sheet` (1-based) of `b` to `path` as delimited text.
 * Returns 0 on success. */
int wubucell_write_csv(const wubucell_book *b, int sheet, char delim, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_CELL_CSV_H */
