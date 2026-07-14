/* odf.h -- OpenDocument Format (ODF / ISO 26300) read + write.
 *
 * ODF files are ZIP containers with a specific shape:
 *   - "mimetype"                 : the media type, stored FIRST and UNCOMPRESSED
 *                                  (how tools sniff ODF without unzipping)
 *   - "content.xml"              : the document body
 *   - "META-INF/manifest.xml"    : lists every part + its media type
 *   - "styles.xml", "meta.xml"   : optional; we emit minimal versions
 *
 * We reuse the three WuBuOffice models:
 *   .odt  <-> dm_doc          (text: paragraphs, headings, tables)
 *   .ods  <-> wubucell_book   (spreadsheet)
 *   .odp  <-> wubushow_pres   (presentation)
 *
 * Clean-room, from-scratch (SLERM): no third-party ODF code. */

#ifndef WUBUODF_ODF_H
#define WUBUODF_ODF_H

#include <stddef.h>
#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubushow/show.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- writers (model -> .od{t,s,p}) ---- */
int wubuodf_write_odt(const dm_doc *d, const char *path);
int wubuodf_write_ods(const wubucell_book *b, const char *path);
int wubuodf_write_odp(const wubushow_pres *p, const char *path);

/* ---- readers (.od{t,s,p} -> model) ---- */
/* On success *out receives a model the caller frees (wubuedit_docmodel_free /
 * wubucell_free / wubushow_free respectively). Returns 0 on success. */
int wubuodf_read_odt(const char *path, dm_doc *out);
int wubuodf_read_ods(const char *path, wubucell_book **out);
int wubuodf_read_odp(const char *path, wubushow_pres **out);

/* ---- shared package helper (used by all three writers) ---- */
/* Assemble an ODF zip at `path`: writes mimetype (stored), content.xml
 * (deflated), a minimal styles.xml/meta.xml, and META-INF/manifest.xml.
 * `mimetype` e.g. "application/vnd.oasis.opendocument.text". Returns 0 ok. */
int wubuodf_assemble(const char *path, const char *mimetype,
                     const char *content_xml, size_t content_len);

#ifdef __cplusplus
}
#endif

#endif /* WUBUODF_ODF_H */
