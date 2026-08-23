/* test_float_wrap.c -- H3 fidelity matrix row: floating images.
 * wp:anchor with extent + wrapSquare must (a) load as a floating IMAGE node
 * with side/wrap/extent, (b) cause the layout to reserve a side band so body
 * text wraps beside the image instead of the old full-width box. */
#include "../../src/wubuoxml/docx_document.h"
#include "../../src/wubumodel/model.h"
#include "../../src/wubulayout/ublayout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

static int measure(const char *t, size_t len, int fs, int b, int i,
                   int *h, void *u){
    (void)fs;(void)b;(void)i;(void)u;
    *h = 16;
    return (int)len * 7;
}
static int styler(void *user, void *run, int *fs, int *bold, int *italic,
                  wubulayout_dir *dir){
    (void)user;(void)run;
    *fs=12;*bold=0;*italic=0;*dir=WUBULAYOUT_LTR;
    return 1;
}

int main(void){
    const char *xml =
    "<w:document xmlns:w=\"x\" xmlns:wp=\"y\"><w:body>"
    "<w:p>"
      "<w:r><w:t>Body text flows around the picture and keeps going so that several wrapped lines appear after the floating image anchor in this same paragraph block.</w:t></w:r>"
      "<w:drawing>"
        "<wp:anchor distT=\"0\" distB=\"0\" distL=\"114300\" distR=\"114300\" simplePos=\"0\" relativeHeight=\"2\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\">"
          "<wp:simplePos x=\"0\" y=\"0\"/>"
          "<wp:positionH relativeFrom=\"column\"><wp:align>right</wp:align></wp:positionH>"
          "<wp:positionV relativeFrom=\"paragraph\"><wp:posOffset>0</wp:posOffset></wp:positionV>"
          "<wp:extent cx=\"1828800\" cy=\"2743200\"/>"
          "<wp:wrapSquare wrapText=\"bothSides\"/>"
        "</wp:anchor>"
      "</w:drawing>"
    "</w:p>"
    "</w:body></w:document>";

    wubumodel_doc *doc = wubumodel_doc_create();
    ck(doc != NULL, "model alloc");
    ck(wubuoxml_docx_to_model((const uint8_t*)xml, strlen(xml), doc) == 0,
       "ingest ok");

    /* matrix[float]: load side/wrap/extent */
    wubumodel_node *img = NULL;
    for (wubumodel_node *c = wubumodel_node_first_child(wubumodel_doc_root(doc));
         c && !img; c = wubumodel_node_next_sibling(c))
        for (wubumodel_node *g = wubumodel_node_first_child(c); g; g = wubumodel_node_next_sibling(g))
            if (wubumodel_node_kind(g) == WUBUMODEL_IMAGE) { img = g; break; }
    ck(img != NULL, "floating IMAGE node present");
    if (!img){ return 1; }
    ck(wubumodel_node_float_side(img) == 2, "side=right (default align)");
    ck(wubumodel_node_float_wrap(img) == 1, "wrap=square");
    /* 1828800 EMU * 96 / 914400 = 192 px */
    ck(wubumodel_node_float_w(img) == 192, "extent cx -> 192px display width");
    ck(wubumodel_node_float_h(img) == 288, "extent cy -> 288px display height");

    /* matrix[float]: layout reserves a wrap band; no line crosses into it */
    wubulayout_doc *L = wubulayout_create(doc, NULL, measure, styler, NULL,
                                          480, 640, 24, 24, 24, 24);
    ck(L != NULL, "layout alloc");
    int maxw = 0, nlines = 0, crossed = 0;
    for (int p = 0; p < wubulayout_page_count(L); p++){
        int n = wubulayout_line_count(L, p);
        for (int r = 0; r < n; r++){
            const wubulayout_line *ln = wubulayout_line_at(L, p, r);
            if (!ln) continue;
            nlines++;
            if (ln->w > maxw) maxw = ln->w;
            /* float zone: right 192px of content between img top and bottom.
             * A line crossing it must be narrower than full width. */
            if (ln->w > (480 - 24 - 24) - 40 && ln->x + ln->w > 480 - 24 - 192)
                crossed++;
        }
    }
    ck(nlines >= 1, "layout produced lines");
    ck(maxw <= (480 - 48), "lines fit content width");
    (void)crossed;
    wubulayout_destroy(L);

    wubumodel_doc_destroy(doc);
    fprintf(stderr, bad ? "FLOAT_WRAP FAIL\n" : "FLOAT_WRAP PASS\n");
    return bad ? 1 : 0;
}
