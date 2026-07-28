/* csv.h -- CSV parser (EXP-86). Parses RFC-4180-ish CSV into a grid of cells,
 * handling quoted fields with embedded commas/quotes/newlines. Opaque. */
#ifndef WUBUCSV_H
#define WUBUCSV_H

typedef struct Csv Csv;

Csv *csv_create(void);
void csv_destroy(Csv *c);

/* Parse `text`. Returns 1 if parsed (even with 0 rows), 0 on alloc error. */
int  csv_parse(Csv *c, const char *text);

int  csv_rows(const Csv *c);
int  csv_cols(const Csv *c);          /* max columns across rows */
const char *csv_cell(const Csv *c, int row, int col);  /* "" if absent */

#endif /* WUBUCSV_H */
