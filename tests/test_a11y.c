/* test_a11y.c -- wubua11y acceptance test (doc + EPUB-parts audits). */
#include "a11y.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

static int has(a11y_report *r, const char *sub){
    for (int i=0;i<r->count;i++) if (strstr(r->items[i], sub)) return 1;
    return 0;
}

static wubumodel_doc *empty_doc(void){ return wubumodel_doc_create(); }

static wubumodel_doc *titled_doc(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec  = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *tp   = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_style *ts  = wubumodel_style_create();
    wubumodel_style_set_prop(ts, "name", "Title");
    wubumodel_node_set_style(tp, ts);
    wubumodel_node *tr   = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(tr, "My Book");
    wubumodel_node_append(d, tp, tr);
    wubumodel_node_append(d, sec, tp);
    /* a body paragraph */
    wubumodel_node *bp = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *br = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(br, "Some real content here.");
    wubumodel_node_append(d, bp, br);
    wubumodel_node_append(d, sec, bp);
    return d;
}

int main(void){
    /* --- empty doc: must flag no text + no title --- */
    a11y_report r1; wubumodel_doc *e = empty_doc();
    a11y_check_doc(e, 1, 1, &r1);
    CHECK(has(&r1, "no body text"), "empty doc -> no body text");
    CHECK(has(&r1, "no document title"), "empty doc -> no title");
    a11y_report_free(&r1); wubumodel_doc_destroy(e);

    /* --- titled doc with content: only the lang caveat remains --- */
    a11y_report r2; wubumodel_doc *d = titled_doc();
    a11y_check_doc(d, 1, 1, &r2);
    CHECK(!has(&r2, "no body text"), "titled doc has text");
    CHECK(!has(&r2, "no document title"), "titled doc has title");
    CHECK(has(&r2, "language not declared"), "titled doc flags lang (model has none)");
    a11y_report_free(&r2); wubumodel_doc_destroy(d);

    /* --- EPUB parts: good vs bad OPF/nav --- */
    const char *good_opf =
        "<package><metadata>"
        "<dc:title>Book</dc:title>"
        "<dc:language>en</dc:language>"
        "</metadata><manifest>"
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>"
        "</manifest></package>";
    const char *good_nav = "<nav epub:type=\"toc\"><ol><li><a>Ch1</a></li></ol></nav>";
    const char *good_xhtml = "<p>hi</p><img src=\"a.png\" alt=\"A picture\"/>";

    a11y_report r3;
    a11y_check_epub_parts(good_opf, good_nav, good_xhtml, &r3);
    CHECK(r3.count == 0, "good EPUB parts -> 0 issues");
    a11y_report_free(&r3);

    const char *bad_opf = "<package><metadata></metadata></package>";
    const char *bad_nav = "<div>no toc here</div>";
    const char *bad_xhtml = "<img src=\"b.png\"/>";   /* missing alt */
    a11y_report r4;
    a11y_check_epub_parts(bad_opf, bad_nav, bad_xhtml, &r4);
    CHECK(has(&r4, "missing <dc:title>"), "bad opf -> title issue");
    CHECK(has(&r4, "missing <dc:language>"), "bad opf -> language issue");
    CHECK(has(&r4, "properties=\"nav\""), "bad opf -> nav property issue");
    CHECK(has(&r4, "epub:type=\"toc\""), "bad nav -> toc issue");
    CHECK(has(&r4, "image without alt text"), "bad xhtml -> img alt issue");
    a11y_report_free(&r4);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubua11y (doc empty/titled + EPUB parts good/bad)\n");
    return 0;
}
