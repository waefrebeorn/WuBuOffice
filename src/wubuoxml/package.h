#ifndef WUBUOXML_PACKAGE_H
#define WUBUOXML_PACKAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque Open Packaging Conventions (OPC) writer. Layers a valid
 * [Content_Types].xml + .rels structure over a ZIP container, which is what
 * every OOXML document (docx/xlsx/pptx) requires to be recognized by Office
 * and LibreOffice. */
typedef struct wubuoxml_package wubuoxml_package;

/* Create a package that writes its ZIP to `out`. */
wubuoxml_package *wubuoxml_create(FILE *out);

/* Register a default content type for a file extension (e.g. "xml" ->
 * "application/xml"). Required for every distinct extension of a part you add.
 * Must be called before finalize(); returns 0 on success. */
int wubuoxml_add_default_type(wubuoxml_package *p, const char *ext, const char *ct);

/* Register an override content type for a specific part path (e.g.
 * "/word/document.xml" -> the WordprocessingML content type). */
int wubuoxml_add_override(wubuoxml_package *p, const char *part, const char *ct);

/* Add a relationship from a source part to a target. `source` is the part the
 * relationship is rooted at (use "" for the package root). `target` is a
 * relative path inside the package. `rtype` is the relationship type URI. An
 * auto-incrementing numeric Id is assigned (rId1, rId2, ...). */
int wubuoxml_add_relationship(wubuoxml_package *p, const char *source, const char *target, const char *rtype);

/* Add an opaque XML part by path (e.g. "word/document.xml"). The bytes in
 * `data`/`size` are written verbatim into the ZIP. The content type must
 * already have been registered via add_default_type/add_override. Returns 0 on
 * success, -1 on error. */
int wubuoxml_add_part(wubuoxml_package *p, const char *path, const void *data, size_t size);

/* Emit [Content_Types].xml, all _rels parts, and finalize the underlying ZIP.
 * Returns 0 on success, -1 on I/O error. */
int wubuoxml_finalize(wubuoxml_package *p);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_PACKAGE_H */
