/* test_table_styles.c -- H6 fidelity matrix row: table styles.
 * A styles.xml with a basedOn chain + conditional formats must resolve so a
 * header row comes out bold+centered and banded rows alternate shading --
 * with NO direct formatting in the document. */
#include "../../src/wubuoxml/table_styles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

static const char *styles_xml =
"<?xml version=\"1.0\"?>"
"<w:styles xmlns:w=\"x\">"
  "<w:style w:type=\"table\" w:styleId=\"TableNormal\">"
    "<w:name w:val=\"Normal Table\"/>"
  "</w:style>"
  "<w:style w:type=\"table\" w:styleId=\"GridTable\">"
    "<w:basedOn w:val=\"TableNormal\"/>"
    "<w:tblStylePr w:type=\"band2H\">"
      "<w:shd w:fill=\"E7EEF7\"/>"
    "</w:tblStylePr>"
  "</w:style>"
  "<w:style w:type=\"table\" w:styleId=\"GridTableAccent\">"
    "<w:basedOn w:val=\"GridTable\"/>"
    "<w:tblStylePr w:type=\"firstRow\">"
      "<w:rPr><w:b/></w:rPr>"
      "<w:tcPr><w:jc w:val=\"center\"/><w:shd w:fill=\"2E5C8A\"/></w:tcPr>"
    "</w:tblStylePr>"
  "</w:style>"
"</w:styles>";

int main(void){
    size_t n = 0;
    TableStyle *st = table_styles_parse(styles_xml, strlen(styles_xml), &n);
    ck(st != NULL, "parse ok");
    ck(n == 3, "3 table styles collected");
    for (size_t i = 0; i < n; i++)
        fprintf(stderr,"[styles] %s basedOn=%s first=%d b2=%d\n",
                st[i].id, st[i].based_on, st[i].has_first_row, st[i].has_band2h);

    unsigned look = TS_LOOK_FIRST_ROW | TS_LOOK_NO_VBAND;  /* banding ON */

    /* header row (row 0): bold + centered + accent shading via the CHAIN
     * (GridTableAccent -> GridTable -> TableNormal) */
    TblCellProps h = table_styles_resolve(st, n, "GridTableAccent", look, 0);
    ck(h.bold, "header bold (from conditional firstRow)");
    ck(h.centered, "header centered");
    ck(h.shading_r == 0x2E && h.shading_g == 0x5C && h.shading_b == 0x8A,
       "header accent shading");

    /* banded row 1: band1H not defined in chain, band2H shading applies to
     * even data rows (row_index % 2 == 0) */
    TblCellProps b2 = table_styles_resolve(st, n, "GridTableAccent", look, 2);
    ck(!b2.bold, "banded row not bold");
    ck(b2.shading_r == 0xE7 && b2.shading_g == 0xEE && b2.shading_b == 0xF7,
       "banded row band2H shading from basedOn parent");

    /* plain row 1: no band1H defined anywhere -> no shading */
    TblCellProps b1 = table_styles_resolve(st, n, "GridTableAccent", look, 1);
    ck(b1.shading_r == -1, "unbanded row has no fill");

    /* unknown style id resolves to defaults */
    TblCellProps unk = table_styles_resolve(st, n, "NoSuchStyle", look, 0);
    ck(unk.shading_r == -1 && !unk.bold, "unknown style id safe");

    if (st) table_styles_free(st);
    fprintf(stderr, bad ? "TABLE_STYLES FAIL\n" : "TABLE_STYLES PASS\n");
    return bad ? 1 : 0;
}
