/* conv_bridge.h -- cross-family model transforms for WuBuOffice convert.
 *
 * These translate between the three canonical models so any input family can
 * reach any output family:
 *   MODEL (wubumodel_doc) <-> TEXT (dm_doc)
 *   SHEET (wubucell_book) <-> TEXT (dm_doc)
 *   SHOW  (wubushow_pres) <-> TEXT (dm_doc)
 *
 * They build dm_doc blocks directly (the model is opaque to callers but the
 * converter dispatcher is part of the same layer that owns the dm_doc schema),
 * so they live here as their own cohesive module rather than inside the
 * file-format dispatch in conv_map.c.
 *
 * Clean-room C11. */

#ifndef WUBUCONV_CONV_BRIDGE_H
#define WUBUCONV_CONV_BRIDGE_H

#include <stddef.h>
#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubushow/show.h"
#include "../wubumodel/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SHEET -> TEXT: one table per sheet, header row bold. */
void wubuconv_sheet_to_text(const wubucell_book *b, dm_doc *out);
/* SHOW  -> TEXT: per-slide title (heading) + body lines as a paragraph. */
void wubuconv_show_to_text(const wubushow_pres *p, dm_doc *out);
/* TEXT  -> SHEET: flatten paragraphs as rows, table cells into columns. */
void wubuconv_text_to_sheet(const dm_doc *d, wubucell_book *b);
/* TEXT  -> SHOW: each heading starts a slide, following paragraphs are body. */
void wubuconv_text_to_show(const dm_doc *d, wubushow_pres *p);
/* MODEL -> TEXT: walks SECTION->BLOCK->PARAGRAPH->RUN (and TABLE rows/cells),
 * projecting onto the dm_doc shape so any PDF/markdown round-trip downstream
 * works. Headings become Heading1/Heading2/Heading3 styled paragraphs based
 * on depth (DOC-58 named-style lookup). */
void wubuconv_model_to_text(const wubumodel_doc *m, dm_doc *out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCONV_CONV_BRIDGE_H */
