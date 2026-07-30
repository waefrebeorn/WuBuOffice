/* test_conv_model -- headless test for wubuconv_model_to_text.
 *
 * Builds a small wubumodel_doc with a top-level table and a section, then
 * projects it onto the dm_doc shape via wubuconv_model_to_text and asserts
 * the structural blocks survive the translation:
 *   - sections become paragraph blocks with collected run text
 *   - tables become dm_table blocks
 *   - cell text is collected from runs
 *
 * The conv bridge walks top-level nodes via wubumodel_doc_root + next_sibling.
 * We create a parent-less table (it becomes the first root node) and a
 * parent-less section (it becomes the next sibling via append-to-table's
 * parent... no: we need them as siblings of the same parent). Since the model
 * has no root container, we append both to the table itself — the section
 * becomes the table's child, and the conv bridge's table handler walks
 * children as "rows". This won't give a clean table test, so instead we make
 * the table the root and verify the table path, then do a separate test for
 * the paragraph path by making a section the root.
 *
 * Simpler approach: two separate tests in one main(). */
#include "../apps/wubuconv/conv_bridge.h"
#include "../apps/wubuedit/docmodel.h"
#include "../src/wubumodel/model.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static wubumodel_doc *mk_section_with_text(void){
    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) return NULL;
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *p   = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *r1  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(r1, "Hello ");
    wubumodel_node_append(d, p, r1);
    wubumodel_node *r2  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(r2, "World");
    wubumodel_node_append(d, p, r2);
    wubumodel_node_append(d, sec, p);
    return d;
}

static wubumodel_doc *mk_table_with_cells(void){
    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) return NULL;
    wubumodel_node *tbl = wubumodel_node_create(d, WUBUMODEL_TABLE);
    for (int r = 0; r < 2; r++){
        wubumodel_node *cell = wubumodel_node_create(d, WUBUMODEL_CELL);
        wubumodel_node *para = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
        wubumodel_node *rr   = wubumodel_node_create(d, WUBUMODEL_RUN);
        char buf[32]; snprintf(buf, sizeof buf, "Cell %d", r + 1);
        wubumodel_run_set_text(rr, buf);
        wubumodel_node_append(d, para, rr);
        wubumodel_node_append(d, cell, para);
        wubumodel_node_append(d, tbl, cell);
    }
    return d;
}

int main(void){
    int fails = 0;

    /* ---- Test 1: section with paragraph + runs → paragraph block ---- */
    {
        wubumodel_doc *m = mk_section_with_text();
        if (!m){ fprintf(stderr, "[test1] model alloc failed\n"); return 1; }
        dm_doc out;
        wubuconv_model_to_text(m, &out);

        int npara = 0, ntable = 0;
        for (size_t i = 0; i < out.n; i++){
            if (out.blocks[i].kind == DM_BLOCK_PARA) npara++;
            if (out.blocks[i].kind == DM_BLOCK_TABLE) ntable++;
        }

        if (npara < 1){ fprintf(stderr, "[test1] expected >=1 paragraph, got %d\n", npara); fails++; }
        if (ntable != 0){ fprintf(stderr, "[test1] expected 0 tables, got %d\n", ntable); fails++; }

        int found_text = 0;
        for (size_t i = 0; i < out.n; i++){
            if (out.blocks[i].kind == DM_BLOCK_PARA && out.blocks[i].para.text){
                if (strstr(out.blocks[i].para.text, "Hello") && strstr(out.blocks[i].para.text, "World")){
                    found_text = 1; break;
                }
            }
        }
        if (!found_text){ fprintf(stderr, "[test1] 'Hello World' not found\n"); fails++; }

        wubuedit_docmodel_free(&out);
        wubumodel_doc_destroy(m);
    }

    /* ---- Test 2: top-level table → table block ---- */
    {
        wubumodel_doc *m = mk_table_with_cells();
        if (!m){ fprintf(stderr, "[test2] model alloc failed\n"); return 1; }
        dm_doc out;
        wubuconv_model_to_text(m, &out);

        int npara = 0, ntable = 0;
        for (size_t i = 0; i < out.n; i++){
            if (out.blocks[i].kind == DM_BLOCK_PARA) npara++;
            if (out.blocks[i].kind == DM_BLOCK_TABLE) ntable++;
        }

        if (ntable != 1){ fprintf(stderr, "[test2] expected 1 table, got %d\n", ntable); fails++; }

        /* Check cell text survived.
         * The conv bridge treats table children as "rows" and their children as
         * "cells". Since cells are direct children of the table, each cell
         * becomes a 1x1 "row" with its paragraph/run as the cell content. */
        int found_cell = 0;
        for (size_t i = 0; i < out.n; i++){
            if (out.blocks[i].kind == DM_BLOCK_TABLE){
                dm_table *t = &out.blocks[i].table;
                for (size_t r = 0; r < t->rows; r++)
                    for (size_t c = 0; c < t->cols; c++){
                        dm_para *cell = t->cells[r * t->cols + c];
                        if (cell && cell->text && strstr(cell->text, "Cell")){
                            found_cell = 1; break;
                        }
                    }
            }
        }
        if (!found_cell){ fprintf(stderr, "[test2] 'Cell' text not found in table\n"); fails++; }

        wubuedit_docmodel_free(&out);
        wubumodel_doc_destroy(m);
    }

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: conv_model (model->text: section→paragraph, table→table block)\n");
    return 0;
}
