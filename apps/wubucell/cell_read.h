#ifndef WUBUCELL_CELL_READ_H
#define WUBUCELL_CELL_READ_H

#include <stddef.h>
#include <stdint.h>
#include "cell.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read an .xlsx file into a wubucell_book. Shared-string refs (t="s"),
 * inline strings (t="inlineStr"/t="str"), numbers and formulas (with their
 * cached <v>) are all captured. Returns 0 on success, -1 on error.
 * The caller owns the returned book (free with wubucell_free). */
int wubucell_read(const char *path, wubucell_book **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCELL_CELL_READ_H */
