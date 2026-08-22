/* test_grabbag.c -- F1 fidelity doctrine: unknown constructs must be
 * PRESERVED (WUBUMODEL_FOREIGN with verbatim raw XML), never dropped.
 * Round-trip loss is the #1 "broken formatting" complaint; this test makes
 * silent data loss a build failure. */
#include "../../src/wubuoxml/docx_document.h"
#include "../../src/wubumodel/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

static int nforeign=0, nsdt=0, has_sdtcontent=0, found_hello=0, found_after=0;

static void walk(wubumodel_node *n){
    for (wubumodel_node *c = n; c; c = wubumodel_node_next_sibling(c)){
        if (wubumodel_node_kind(c) == WUBUMODEL_FOREIGN){
            nforeign++;
            const char *nm  = wubumodel_node_foreign_name(c);
            const char *raw = wubumodel_node_foreign_raw(c);
            if (nm && strstr(nm, "sdt")) nsdt = 1;
            if (raw && strstr(raw, "sdtContent")) has_sdtcontent = 1;
        }
        const char *t = wubumodel_run_text(c);
        if (t){
            if (strstr(t, "Hello")) found_hello = 1;
            if (strstr(t, "after")) found_after = 1;
        }
        walk(wubumodel_node_first_child(c));
    }
}

int main(void){
    const char *xml =
    "<?xml version=\"1.0\"?>"
    "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
      "<w:body>"
        "<w:p><w:r><w:t>Hello </w:t></w:r>"
          "<w:sdt><w:sdtPr><w:alias w:val=\"date\"/></w:sdtPr>"
            "<w:sdtContent><w:r><w:t>2026-08-22</w:t></w:r></w:sdtContent></w:sdt>"
        "</w:p>"
        "<w:p><w:r><w:t>after</w:t></w:r></w:p>"
      "</w:body>"
    "</w:document>";

    wubumodel_doc *doc = wubumodel_doc_create();
    if (!doc){ fprintf(stderr,"model alloc failed\n"); return 1; }
    int rc = wubuoxml_docx_to_model((const uint8_t *)xml, strlen(xml), doc);
    ck(rc == 0, "parse ok");

    walk(wubumodel_doc_root(doc));

    ck(nforeign >= 1, "at least one FOREIGN node preserved");
    ck(nsdt, "unknown sdt element named");
    ck(has_sdtcontent, "raw XML includes nested sdtContent");
    ck(found_hello, "known text before unknown element intact");
    ck(found_after, "known text after unknown element intact");

    wubumodel_doc_destroy(doc);
    fprintf(stderr, bad ? "GRABBAG FAIL\n" : "GRABBAG PASS\n");
    return bad ? 1 : 0;
}
