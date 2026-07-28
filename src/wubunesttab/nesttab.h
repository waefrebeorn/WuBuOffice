/* nesttab.h -- nested tables (DOC-75). Builder + validator over wubumodel:
 * constructs TABLE -> CELL grids, nests a child TABLE inside any CELL, walks
 * nesting depth, and validates structure (TABLE children are CELLs, cell
 * counts consistent per declared columns). Opaque. */
#ifndef WUBUNESTTAB_H
#define WUBUNESTTAB_H

/* Create a rows x cols table under `parent` (both wubumodel types; parent may
 * be a SECTION/CELL/etc.). Each cell gets an empty RUN. Returns the TABLE
 * node, or NULL on error. */
void *nesttab_build(void *doc, void *parent, int rows, int cols);

/* Nest a rows x cols child table inside `cell` (must be a CELL). Returns the
 * child TABLE node or NULL. */
void *nesttab_nest(void *doc, void *cell, int rows, int cols);

/* Max table-nesting depth at/under `node` (a TABLE directly under a CELL of
 * another TABLE counts as depth+1). A lone table = 1; no table = 0. */
int nesttab_depth(const void *node);

/* Validate: every child of a TABLE is a CELL. Returns 1 valid, 0 invalid. */
int nesttab_validate(const void *table);

/* Cell at (row,col) of `table` given the column count it was built with,
 * or NULL if out of range. */
void *nesttab_cell(void *table, int row, int col, int cols);

#endif /* WUBUNESTTAB_H */
