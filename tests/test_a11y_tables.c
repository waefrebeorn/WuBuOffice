/* test_a11y_tables.c -- hop 20: table accessibility checks (MS checker
 * parity). Build a doc with a problematic table; assert the auditor flags:
 *   - merged cells (gridSpan/vMerge)
 *   - a completely blank row
 *   - single-row table (no header row)
 * And that a well-formed table produces no table issues. */
#include "../../src/wubua11y/a11y.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

static wubumodel_doc *build_doc_with_table(int nrows, int ncols,
                                           int merge_first, int blank_row2){
    wubumodel_doc *doc = wubumodel_doc_create();
    /* canonical structure: SECTION is the parentless root */
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    wubumodel_node *tbl = wubumodel_node_create(doc, WUBUMODEL_TABLE);
    wubumodel_node_append(doc, sec, tbl);
    for (int r = 0; r < nrows; r++){
        wubumodel_node *row = wubumodel_node_create(doc, WUBUMODEL_BLOCK);
        wubumodel_node_append(doc, tbl, row);
        for (int c = 0; c < ncols; c++){
            wubumodel_node *cell = wubumodel_node_create(doc, WUBUMODEL_CELL);
            wubumodel_node_append(doc, row, cell);
            if (merge_first && r == 0 && c == 0)
                wubumodel_node_set_span(cell, 2, 0);
            wubumodel_node *para = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
            wubumodel_node_append(doc, cell, para);
            if (!(blank_row2 && r == 1)){
                wubumodel_node *run = wubumodel_node_create(doc, WUBUMODEL_RUN);
                wubumodel_node_append(doc, para, run);
                char buf[16]; snprintf(buf, sizeof buf, "r%dc%d", r, c);
                wubumodel_run_set_text(run, buf);
            }
            if (merge_first && r == 0 && c == 1) break;  /* spanned over */
        }
    }
    return doc;
}

static int report_has(const a11y_report *r, const char *needle){
    for (int i = 0; i < r->count; i++)
        if (strstr(r->items[i], needle)) return 1;
    return 0;
}

int main(void){
    /* 1. merged cell + 2 rows: expect merge warning */
    wubumodel_doc *d1 = build_doc_with_table(2, 2, 1, 0);
    a11y_report r1;
    a11y_check_doc(d1, 0, 0, &r1);
    ck(report_has(&r1, "merged/split cell"), "merged cell flagged");
    a11y_report_free(&r1);
    wubumodel_doc_destroy(d1);

    /* 2. blank second row */
    wubumodel_doc *d2 = build_doc_with_table(2, 2, 0, 1);
    a11y_report r2;
    a11y_check_doc(d2, 0, 0, &r2);
    ck(report_has(&r2, "completely blank"), "blank row flagged");
    a11y_report_free(&r2);
    wubumodel_doc_destroy(d2);

    /* 3. single-row table: no header row */
    wubumodel_doc *d3 = build_doc_with_table(1, 2, 0, 0);
    a11y_report r3;
    a11y_check_doc(d3, 0, 0, &r3);
    ck(report_has(&r3, "header row"), "single-row table flagged");
    a11y_report_free(&r3);
    wubumodel_doc_destroy(d3);

    /* 4. clean 2x2 table: no table issues */
    wubumodel_doc *d4 = build_doc_with_table(2, 2, 0, 0);
    a11y_report r4;
    a11y_check_doc(d4, 0, 0, &r4);
    ck(!report_has(&r4, "merged/split"), "clean table: no merge flag");
    ck(!report_has(&r4, "blank"), "clean table: no blank flag");
    ck(!report_has(&r4, "header row"), "clean table: no header flag");
    a11y_report_free(&r4);
    wubumodel_doc_destroy(d4);

    fprintf(stderr, bad ? "A11Y_TABLES FAIL\n" : "A11Y_TABLES PASS\n");
    return bad ? 1 : 0;
}
