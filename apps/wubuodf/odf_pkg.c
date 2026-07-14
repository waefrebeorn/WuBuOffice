/* odf_pkg.c -- shared ODF package assembler. See odf.h.
 * Clean-room C11 over the raw wubuzip writer. */

#include "odf.h"
#include "../../src/wubuzip/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal manifest listing the parts we emit for `mimetype`. */
static char *build_manifest(const char *mimetype) {
    static const char *fmt =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.3\">\n"
        " <manifest:file-entry manifest:full-path=\"/\" manifest:media-type=\"%s\"/>\n"
        " <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
        " <manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>\n"
        " <manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>\n"
        "</manifest:manifest>\n";
    size_t n = strlen(fmt) + strlen(mimetype) + 1;
    char *s = malloc(n);
    if (s) snprintf(s, n, fmt, mimetype);
    return s;
}

static const char *STYLES_XML =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<office:document-styles "
    "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
    "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
    "xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" "
    "office:version=\"1.3\">\n"
    "<office:styles/>\n"
    "</office:document-styles>\n";

static const char *META_XML =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<office:document-meta "
    "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
    "xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\" "
    "office:version=\"1.3\">\n"
    "<office:meta><meta:generator>WuBuOffice</meta:generator></office:meta>\n"
    "</office:document-meta>\n";

int wubuodf_assemble(const char *path, const char *mimetype,
                     const char *content_xml, size_t content_len) {
    FILE *out = fopen(path, "wb");
    if (!out) return -1;
    wubuzip_writer *z = wubuzip_create(out);
    if (!z) { fclose(out); return -1; }

    int rc = 0;
    /* CRITICAL: mimetype must be the FIRST entry and STORED (uncompressed),
     * so a reader can sniff the type from a fixed offset. */
    rc |= wubuzip_add(z, "mimetype", mimetype, (uint32_t)strlen(mimetype));
    rc |= wubuzip_add_deflated(z, "content.xml", content_xml, (uint32_t)content_len);
    rc |= wubuzip_add_deflated(z, "styles.xml", STYLES_XML, (uint32_t)strlen(STYLES_XML));
    rc |= wubuzip_add_deflated(z, "meta.xml", META_XML, (uint32_t)strlen(META_XML));

    char *manifest = build_manifest(mimetype);
    if (manifest) {
        rc |= wubuzip_add_deflated(z, "META-INF/manifest.xml", manifest, (uint32_t)strlen(manifest));
        free(manifest);
    } else rc = -1;

    rc |= wubuzip_finalize(z);
    fclose(out);
    return rc;
}
