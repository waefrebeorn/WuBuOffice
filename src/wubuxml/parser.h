#ifndef WUBUXML_PARSER_H
#define WUBUXML_PARSER_H

/* Minimal, dependency-free, streaming XML event parser (ws05#0884).
 * SAX-style: you supply a handler; the parser emits element-open /
 * element-close / text events as it scans. Namespaces are KEPT on
 * names (e.g. "w:p", "w:t") so callers match WordprocessingML.
 *
 * Not a full XML validator: tuned for the well-formed documents WE
 * emit and the OOXML we read. Handles self-closing tags, attributes
 * (incl. namespaced), character data, and the common entities
 * (&amp; &lt; &gt; &quot; &apos;). */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WUBUXML_EVT_START,   /* an element opened: name/attr_set valid */
    WUBUXML_EVT_END,     /* an element closed: name valid */
    WUBUXML_EVT_TEXT     /* character data: text valid (entity-decoded) */
} wubuxml_event;

#define WUBUXML_MAX_ATTR 16

typedef struct {
    const char *name;                       /* element/attr local-or-ns name */
    const char *attr_name[WUBUXML_MAX_ATTR];
    const char *attr_val[WUBUXML_MAX_ATTR];
    int         attr_count;
    const char *text;                        /* valid for EVT_TEXT only */
    size_t      text_len;
} wubuxml_info;

/* Return 0 to continue, non-zero to abort the parse (propagated). */
typedef int (*wubuxml_handler)(wubuxml_event evt,
                               const wubuxml_info *info, void *user);

/* Parse `len` bytes at `data`. Returns 0 on success (or the
 * non-zero value a handler returned to abort), -1 on a structural
 * error (unbalanced tags, bad markup). */
int wubuxml_parse(const uint8_t *data, size_t len,
                   wubuxml_handler h, void *user);

#ifdef __cplusplus
}
#endif
#endif /* WUBUXML_PARSER_H */
