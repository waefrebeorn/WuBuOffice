/* model_json.h -- JSON serializers for all three WuBuOffice models.
 *
 * Lossless (for the structure each model captures) JSON dumps so any office
 * document can feed a data pipeline. One function per model:
 *   dm_doc          -> document JSON (blocks: paragraphs + tables)
 *   wubucell_book   -> workbook JSON (sheets -> cells with kind/value)
 *   wubushow_pres   -> presentation JSON (slides -> title/body)
 *
 * Clean-room, from-scratch (SLERM): no third-party JSON code. */

#ifndef WUBUDOC_MODEL_JSON_H
#define WUBUDOC_MODEL_JSON_H

#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubushow/show.h"

#ifdef __cplusplus
extern "C" {
#endif

int wubudoc_write_doc_json(const dm_doc *d, const char *path);
int wubudoc_write_book_json(const wubucell_book *b, const char *path);
int wubudoc_write_pres_json(const wubushow_pres *p, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUDOC_MODEL_JSON_H */
