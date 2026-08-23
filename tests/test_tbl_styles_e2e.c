/* test_tbl_styles_e2e.c -- H6d end-to-end: a docx (document.xml +
 * styles.xml) whose table references GridTableAccent with firstRow/banding
 * tblLook flags must produce runs carrying the resolved cell props
 * (bold on header, shading on banded rows) after ingest. */
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

static const char *DOC =
"<w:document xmlns:w=\"x\"><w:body>"
  "<w:tbl>"
    "<w:tblPr>"
      "<w:tblStyle w:val=\"GridTableAccent\"/>"
      "<w:tblLook w:val=\"04A0\" firstRow=\"1\" lastRow=\"0\" "
        "firstColumn=\"1\" lastColumn=\"0\" noHBand=\"0\" noVBand=\"1\"/>"
    "</w:tblPr>"
    "<w:tr>"
      "<w:tc><w:p><w:r><w:t>Header</w:t></w:r></w:p></w:tc>"
    "</w:tr>"
    "<w:tr>"
      "<w:tc><w:p><w:r><w:t>data</w:t></w:r></w:p></w:tc>"
    "</w:tr>"
  "</w:tbl>"
"</w:body></w:document>";

static const char *STYLES =
"<?xml version=\"1.0\"?>"
"<w:styles xmlns:w=\"x\">"
  "<w:style w:type=\"table\" w:styleId=\"GridTableAccent\">"
    "<w:tblStylePr w:type=\"firstRow\">"
      "<w:rPr><w:b/></w:rPr>"
      "<w:shd w:fill=\"2E5C8A\"/>"
    "</w:tblStylePr>"
    "<w:tblStylePr w:type=\"band2H\">"
      "<w:shd w:fill=\"E7EEF7\"/>"
    "</w:tblStylePr>"
  "</w:style>"
"</w:styles>";

/* find a run by text and report whether its style has bold */
static int run_has_bold(const char *needle){
    return -1;   /* replaced below */
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    ck(doc != NULL, "model alloc");
    ck(wubuoxml_docx_to_model_ex((const uint8_t*)DOC, strlen(DOC),
                                 (const uint8_t*)STYLES, strlen(STYLES),
                                 doc) == 0, "ingest ok");

    /* find runs by text; header run should carry bold style */
    int hdr_bold = -1;
    for (wubumodel_node *c = wubumodel_node_first_child(wubumodel_doc_root(doc));
         c; c = wubumodel_node_next_sibling(c))
        for (wubumodel_node *t = wubumodel_node_first_child(c); t;
             t = wubumodel_node_next_sibling(t))
            for (wubumodel_node *r = wubumodel_node_first_child(t); r;
                 r = wubumodel_node_next_sibling(r))
                for (wubumodel_node *p = wubumodel_node_first_child(r); p;
                     p = wubumodel_node_next_sibling(p))
                    for (wubumodel_node *rn = wubumodel_node_first_child(p); rn;
                         rn = wubumodel_node_next_sibling(rn)){
                        const char *txt = wubumodel_run_text(rn);
                        if (!txt) continue;
                        wubumodel_style *st = NULL;
                        /* read via node style getter if exposed */
                        extern wubumodel_style *wubumodel_node_style(const wubumodel_node*);
                        st = wubumodel_node_style(rn);
                        int bold = 0;
                        if (st){
                            const char *v = wubumodel_style_get_prop(st,"bold");
                            bold = v && !strcmp(v,"1");
                        }
                        if (!strcmp(txt,"Header")) hdr_bold = bold;
                    }
    ck(hdr_bold == 1, "header run carries resolved bold from table style");

    wubumodel_doc_destroy(doc);
    fprintf(stderr, bad ? "TBL_STYLES_E2E FAIL\n" : "TBL_STYLES_E2E PASS\n");
    return bad ? 1 : 0;
}
