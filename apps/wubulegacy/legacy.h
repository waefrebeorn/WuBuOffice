/* wubulegacy -- legacy binary Microsoft Office formats: read AND write.
 *   .xls  (BIFF8 records)          -> wubucell_book        (read + write)
 *   .doc  (Word binary, FIB+text)  -> dm_doc                (read + write)
 *   .ppt  (PowerPoint binary atoms)-> wubushow_pres         (read + write)
 *
 * All three sit on the shared MS-CFB container (src/wubucfb). Writers produce
 * minimal-but-valid files that Excel / Word / PowerPoint and LibreOffice open,
 * and that our own readers round-trip exactly. Clean-room C11, one cohesive
 * module per format. */

#ifndef WUBULEGACY_LEGACY_H
#define WUBULEGACY_LEGACY_H

#include "../wubucell/cell.h"
#include "../wubushow/show.h"
#include "../wubuedit/docmodel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- readers ---- */
int wubulegacy_read_xls(const char *path, wubucell_book **out);
int wubulegacy_read_doc(const char *path, dm_doc *out);
int wubulegacy_read_ppt(const char *path, wubushow_pres **out);

/* ---- writers ---- */
int wubulegacy_write_xls(const wubucell_book *bk, const char *path);
int wubulegacy_write_doc(const dm_doc *d, const char *path);
int wubulegacy_write_ppt(const wubushow_pres *p, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBULEGACY_LEGACY_H */
