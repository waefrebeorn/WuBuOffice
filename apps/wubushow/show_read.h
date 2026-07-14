/* show_read.h -- read a .pptx into a wubushow_pres.
 *
 * SAX over wubuxml_parse. We resolve the slide list from ppt/presentation.xml
 * (sldIdLst order + r:id) via ppt/_rels/presentation.xml.rels, then parse each
 * ppt/slides/slideN.xml. Inside a slide, the shape whose cNvPr name is "Title"
 * contributes the slide title; every other <a:t> run (the body bullets) is
 * concatenated into the body, joined on newlines to mirror how the writer
 * emits bullets (one <a:p> per line).
 *
 * Opaque to callers; the wubushow_pres is the same struct the writer consumes,
 * so a read->assemble loop is a true structure-preserving round-trip.
 *
 * Clean-room, from-scratch (SLERM). */

#ifndef WUBUSSHOW_SHOW_READ_H
#define WUBUSSHOW_SHOW_READ_H

#include <stddef.h>
#include <stdint.h>

#include "show.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read an .pptx file into a wubushow_pres. Returns 0 on success, -1 on error.
 * The caller owns the returned presentation (free with wubushow_free). */
int wubushow_read(const char *path, wubushow_pres **out);

#ifdef __cplusplus
}
#endif

#endif /* WUBUSSHOW_SHOW_READ_H */
