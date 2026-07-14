/* odf_body.h -- shared ODF content-body emitters + string buffer helpers.
 *
 * The three ODF document classes (text/sheet/presentation) share:
 *   - a growable, XML-escaping string buffer (sbuf), and
 *   - the <office:body> inner XML that is IDENTICAL whether the document is
 *     packaged (.odt/.ods/.odp, a ZIP with content.xml) or FLAT
 *     (.fodt/.fods/.fodp, a single self-contained XML file).
 *
 * Centralizing them here means the packaged writers (odf_{text,sheet,pres}.c)
 * and the flat writers (odf_flat.c) emit byte-identical bodies from one source
 * of truth -- reuse, never duplicate (soul.md NO MONOLITHS). */

#ifndef WUBUODF_ODF_BODY_H
#define WUBUODF_ODF_BODY_H

#include <stddef.h>
#include <stdint.h>
#include "odf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- growable string buffer with XML escaping ---- */
typedef struct { char *s; size_t n, cap; } odf_sbuf;
void odf_sb_putn(odf_sbuf *b, const char *p, size_t n);
void odf_sb_puts(odf_sbuf *b, const char *s);
void odf_sb_esc(odf_sbuf *b, const char *s);   /* escapes & < > */

/* The full set of ODF namespace declarations, usable on either the
 * <office:document-content> (packaged) or <office:document> (flat) root.
 * Declaring a superset is harmless and keeps one source of truth. */
extern const char *WUBUODF_NS_ALL;

/* Emit <office:body><office:{text,spreadsheet,presentation}>...</...></office:body>
 * into `b`. These are the exact bodies the packaged writers used to inline. */
void wubuodf_emit_text_body(odf_sbuf *b, const dm_doc *d);
void wubuodf_emit_sheet_body(odf_sbuf *b, const wubucell_book *bk);
void wubuodf_emit_pres_body(odf_sbuf *b, const wubushow_pres *pres);

/* Shared XML-bytes readers: run each ODF SAX handler over raw XML `bytes`.
 * Defined in odf_{text,sheet,pres}.c; reused by the flat .fod? readers so the
 * parse logic lives in exactly one place per document class. */
int wubuodf_parse_text_xml(const uint8_t *bytes, size_t len, dm_doc *out);
int wubuodf_parse_sheet_xml(const uint8_t *bytes, size_t len, wubucell_book *bk);
int wubuodf_parse_pres_xml(const uint8_t *bytes, size_t len, wubushow_pres *pres);

#ifdef __cplusplus
}
#endif

#endif /* WUBUODF_ODF_BODY_H */
