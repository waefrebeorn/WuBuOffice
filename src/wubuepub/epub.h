/* epub.h -- dependency-free C11 EPUB3 writer (wubuepub).
 *
 * Packages a wubumodel_doc into a valid EPUB 3.0 container:
 *   mimetype (STORED, first) + META-INF/container.xml + OEBPS/content.opf
 *   + OEBPS/nav.xhtml + one OEBPS/chap_N.xhtml per top-level SECTION.
 *
 * Model mapping: SECTION -> chapter; PARAGRAPH -> <p> (or <hN> when the
 * paragraph's style name is "Heading N"/"Title"); RUN -> text; TABLE/CELL ->
 * <table>/<td>; LINK -> <a>. Self-contained: from-scratch ZIP + XML, no deps. */
#ifndef WUBUOFFICE_EPUB_H
#define WUBUOFFICE_EPUB_H

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Write `doc` to an EPUB3 file at `path`. `title`/`lang` populate OPF
 * metadata (fallback to "Untitled"/"en" when NULL). Returns 0 ok, -1 error. */
int epub_write(const wubumodel_doc *doc, const char *path,
               const char *title, const char *lang);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_EPUB_H */
