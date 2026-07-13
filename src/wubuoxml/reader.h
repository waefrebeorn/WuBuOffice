#ifndef WUBUOXML_READER_H
#define WUBUOXML_READER_H

#include <stddef.h>
#include <stdint.h>
#include "docx_text.h"

#ifdef __cplusplus
extern "C" {
#endif

/* From-scratch OPC reader built on the ZIP reader. Enumerates parts, parses
 * [Content_Types].xml and the .rels graphs, and extracts plain text from
 * WordprocessingML / SpreadsheetML / PresentationML parts. */

typedef struct wubuoxml_part {
    char *name;        /* part path, e.g. "word/document.xml" */
    char *content_type;
    uint8_t *bytes;    /* inflated content (owned) */
    size_t len;
    char **rel_targets;  /* targets from .rels rooted at this part */
    char *rel_id;        /* rIds match targets by parallel array */
    size_t nrel;
} wubuoxml_part;

typedef struct wubuoxml_package {
    wubuoxml_part *parts;
    size_t n;
} wubuoxml_package;

/* Read an in-memory ZIP buffer as an OPC package. Returns 0 on success. */
int wubuoxml_read(const uint8_t *data, size_t len, wubuoxml_package *p);

size_t wubuoxml_part_count(const wubuoxml_package *p);
const wubuoxml_part *wubuoxml_part_at(const wubuoxml_package *p, size_t i);
/* Find a part by path (with or without leading '/'); NULL if absent. */
const wubuoxml_part *wubuoxml_part_find(const wubuoxml_package *p, const char *path);

void wubuoxml_free(wubuoxml_package *p);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOXML_READER_H */
