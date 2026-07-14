/* wubulegacy -- readers for the legacy binary Microsoft Office formats:
 *   .xls  (BIFF8 records)          -> wubucell_book
 *   .doc  (Word binary, FIB+text)  -> dm_doc
 *   .ppt  (PowerPoint binary atoms)-> wubushow_pres
 *
 * All three sit on the shared MS-CFB container (src/wubucfb). Read-only:
 * legacy binary is a migration target, not a write target. Clean-room C11,
 * one cohesive module per format. */

#ifndef WUBULEGACY_LEGACY_H
#define WUBULEGACY_LEGACY_H

#include "../wubucell/cell.h"
#include "../wubushow/show.h"
#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read a legacy .xls file into a freshly created wubucell_book.
 * On success sets *out (caller frees with wubucell_free) and returns 0. */
int wubulegacy_read_xls(const char *path, wubucell_book **out);

/* Read a legacy .doc file into a dm_doc (caller frees with
 * wubuedit_docmodel_free). *out is zeroed then filled. Returns 0 on success. */
int wubulegacy_read_doc(const char *path, dm_doc *out);

/* Read a legacy .ppt file into a wubushow_pres (caller frees with
 * wubushow_free). Returns 0 on success. */
int wubulegacy_read_ppt(const char *path, wubushow_pres **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBULEGACY_LEGACY_H */
