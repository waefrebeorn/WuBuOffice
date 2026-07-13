#ifndef WUBUXML_XML_H
#define WUBUXML_XML_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Streaming, well-formed XML writer. Escapes text and attribute values
 * correctly and refuses to emit malformed nesting by closing open start tags
 * as needed. Namespaces are just ordinary element/attribute names
 * (e.g. "w:document", "xmlns:w"). */
typedef struct wubuxml_writer wubuxml_writer;

wubuxml_writer *wubuxml_create(FILE *out);

/* Emit an XML declaration. Call once before the root element. */
void wubuxml_declaration(wubuxml_writer *w);

/* Open an element start tag (not yet closed). Call set_attr() while the tag is
 * open, then text() / nested open() / close() to close it. */
void wubuxml_open(wubuxml_writer *w, const char *name);

/* Add an attribute to the currently open start tag. Must be called before any
 * text or child element is written. */
void wubuxml_set_attr(wubuxml_writer *w, const char *key, const char *val);

/* Write a (escaped) text node. Closes any open start tag first. */
void wubuxml_text(wubuxml_writer *w, const char *text);

/* Close the most recently opened element. A childless element is emitted as a
 * self-closing tag; an element with content gets a closing tag. */
void wubuxml_close(wubuxml_writer *w);

/* Close all still-open elements (handy at shutdown). */
void wubuxml_close_all(wubuxml_writer *w);

/* Flush and free the writer. Does NOT close the underlying FILE. */
void wubuxml_destroy(wubuxml_writer *w);

#ifdef __cplusplus
}
#endif

#endif /* WUBUXML_XML_H */
