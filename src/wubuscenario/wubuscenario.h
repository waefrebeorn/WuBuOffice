/* wubuscenario.h — spreadsheet Scenario manager: named sets of cell
 * substitutions ("what-if" scenarios) that can be applied/restored. */
#ifndef WUBUSCENARIO_H
#define WUBUSCENARIO_H
#include <stddef.h>

typedef struct wubuscenario wubuscenario;

/* A single cell change in a scenario: set cell (r,c) to `value`. */
typedef struct {
    int row, col;
    char *value;  /* owned copy */
} wubuscen_cell;

typedef struct {
    char *name;      /* owned copy */
    wubuscen_cell *cells;
    size_t n;
} wubuscen_entry;

wubuscenario *wubuscenario_create(void);
void wubuscenario_destroy(wubuscenario *s);

/* Add or replace a named scenario. Returns 0 on success. */
int wubuscenario_set(wubuscenario *s, const char *name,
                     const wubuscen_cell *cells, size_t n);

/* Look up a scenario by name; returns entry (owned by s) or NULL. */
const wubuscen_entry *wubuscenario_get(const wubuscenario *s, const char *name);

/* Number of scenarios + i-th name (for enumeration). */
size_t wubuscenario_count(const wubuscenario *s);
const char *wubuscenario_name(const wubuscenario *s, size_t i);

/* Apply scenario: for each cell, call `apply(row,col,value,ud)`. Returns 0. */
int wubuscenario_apply(const wubuscenario *s, const char *name,
                       int (*apply)(int row, int col, const char *value, void *ud),
                       void *ud);

#endif
