/* model_from_json.h -- inverse of model_json.c: rebuild the three WuBuOffice
 * canonical models from their JSON serialization, so an AGI can edit a document
 * as JSON and have it re-created in any format.
 *
 * Schemas (mirror of model_json.c emit):
 *   document : {"type":"document","blocks":[{kind:"paragraph",style,bold,text}
 *                                         |{kind:"table",rows,cols,cells:[[..]]}]}
 *   workbook : {"type":"workbook","sheets":[{name,rows,cols,
 *                                         cells:[{r,c,kind:"str|num|formula",value,cached}]}]}
 *   pres     : {"type":"presentation","slides":[{title,body}]}
 *
 * Clean-room C11; reuses wubujson (same j_* API as WuBuPad) for parsing. */
#ifndef WUBUCONV_MODEL_FROM_JSON_H
#define WUBUCONV_MODEL_FROM_JSON_H

#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubushow/show.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse a document-model JSON string into a dm_doc. Returns owned dm_doc*
 * (free with wubuedit_docmodel_free) or NULL on malformed input. */
dm_doc *wubuconv_doc_from_json(const char *json);

/* Parse a workbook-model JSON string into a wubucell_book. Returns owned
 * book* (free with wubucell_free) or NULL. */
wubucell_book *wubuconv_book_from_json(const char *json);

/* Parse a presentation-model JSON string into a wubushow_pres. Returns owned
 * pres* (free with wubushow_free) or NULL. */
wubushow_pres *wubuconv_pres_from_json(const char *json);

#ifdef __cplusplus
}
#endif

#endif /* WUBUCONV_MODEL_FROM_JSON_H */
