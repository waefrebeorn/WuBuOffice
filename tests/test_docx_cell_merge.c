/* test_cell_merge.c -- H6b fidelity matrix row: merged cells.
 * w:gridSpan (horizontal) and w:vMerge (vertical) must load onto the CELL
 * node so the renderer can span columns / suppress continuation cells.
 * Merged cells are a top docx-fidelity breaker. */
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

static int ncells = 0;
static int found_span2 = 0, found_restart = 0, found_continue = 0, found_plain = 0;

static void walk(wubumodel_node *n){
    for (wubumodel_node *c = n; c; c = wubumodel_node_next_sibling(c)){
        if (wubumodel_node_kind(c) == WUBUMODEL_CELL){
            ncells++;
            int sp = wubumodel_node_col_span(c);
            int vm = wubumodel_node_vmerge(c);
            if (sp == 2 && vm == 1) found_span2 = 1;       /* origin of merge */
            if (sp == 1 && vm == 0) found_plain = 1;        /* normal cell */
        }
        walk(wubumodel_node_first_child(c));
    }
}


static void walk2(wubumodel_node *n, int *cont){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_CELL
            && wubumodel_node_vmerge(n) == 2) *cont = 1;
        walk2(wubumodel_node_first_child(n), cont);
    }
}
void count_cont(wubumodel_doc *d, int *cont){
    walk2(wubumodel_doc_root(d), cont);
}

int main(void){
    const char *xml =
    "<w:document xmlns:w=\"x\"><w:body>"
      "<w:tbl>"
        "<w:tr>"
          "<w:tc><w:tcPr><w:gridSpan w:val=\"2\"/><w:vMerge w:val=\"restart\"/></w:tcPr>"
            "<w:p><w:r><w:t>A</w:t></w:r></w:p></w:tc>"
          "<w:tc><w:p><w:r><w:t>B</w:t></w:r></w:p></w:tc>"
        "</w:tr>"
        "<w:tr>"
          "<w:tc><w:tcPr><w:gridSpan w:val=\"2\"/><w:vMerge/></w:tcPr></w:tc>"
          "<w:tc><w:p><w:r><w:t>D</w:t></w:r></w:p></w:tc>"
        "</w:tr>"
      "</w:tbl>"
    "</w:body></w:document>";

    wubumodel_doc *doc = wubumodel_doc_create();
    ck(doc != NULL, "model alloc");
    ck(wubuoxml_docx_to_model((const uint8_t*)xml, strlen(xml), doc) == 0,
       "ingest ok");

    walk(wubumodel_doc_root(doc));
    ck(ncells >= 4, "all cells loaded");
    ck(found_span2, "gridSpan=2 + vMerge restart on origin cell");
    ck(found_plain, "plain cell has span=1 vmerge=none");
    /* continuation cell keeps its geometry flags for the renderer */
    int cont_ok = 0;
    static void *dummy;
    (void)dummy;
    /* full walk for vmerge==2 */
    {
        /* reuse walk-like recursion */
        found_continue = 0;
        // recursive lambda not available in C; do an explicit stackless second pass
    }
    extern void count_cont(wubumodel_doc*, int*);
    count_cont(doc, &cont_ok);
    ck(cont_ok, "vMerge continue recorded");

    wubumodel_doc_destroy(doc);
    fprintf(stderr, bad ? "CELL_MERGE FAIL\n" : "CELL_MERGE PASS\n");
    return bad ? 1 : 0;
}
