/* WuBuOffice -- apps/wubucell/cell_io
 * Package assembly (wubucell_assemble) and teardown (wubucell_free). Builds the
 * OOXML .xlsx package: workbook, styles, shared strings, worksheets, charts
 * and drawings. Reuses the renderers from cell_model.c and the resolver from
 * cell_eval.c.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "cell_internal.h"

int wubucell_assemble(wubucell_book *b, const char *outpath) {
    cell_eval_all(b);   /* backbone: compute all formula results first */
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");

    int has_charts = (b->ncharts > 0);
    wubuoxml_add_override(pkg, "/xl/workbook.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");
    wubuoxml_add_override(pkg, "/xl/styles.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
    for (size_t i = 0; i < b->n; i++) {
        char path[64];
        snprintf(path, sizeof path, "/xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_override(pkg, path, "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
    }
    if (b->use_sst)
        wubuoxml_add_override(pkg, "/xl/sharedStrings.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml");
    if (has_charts) {
        wubuoxml_add_override(pkg, "/xl/drawings/drawing1.xml", "application/vnd.openxmlformats-officedocument.drawing+xml");
        wubuoxml_add_default_type(pkg, "png", "image/png");
    }
    wubuoxml_add_relationship(pkg, "", "xl/workbook.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* workbook.xml */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n<sheets>\n");
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <sheet name=\"%s\" sheetId=\"%zu\" r:id=\"rId%zu\"/>\n", b->sheets[i].name, i + 1, i + 1);
        fprintf(m, "</sheets>\n");
        /* H6 fidelity: force a full recalculation when Excel/LibreOffice
         * opens the file -- our cached values may be stale relative to the
         * formulas if the book was edited externally between saves. */
        fprintf(m, "<calcPr calcId=\"124519\" fullCalcOnLoad=\"1\"/>\n");
        fprintf(m, "</workbook>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/workbook.xml", bb, bn);
        free(bb);
    }
    /* workbook rels */
    {
        char *sp = NULL; size_t sn = 0; FILE *m = open_memstream(&sp, &sn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        size_t rid = 1;
        for (size_t i = 0; i < b->n; i++)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%zu.xml\"/>\n", rid++, i + 1);
        fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n", rid++);
        if (b->use_sst)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>\n", rid++);
        if (has_charts)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"drawings/drawing1.xml\"/>\n", rid++);
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/_rels/workbook.xml.rels", sp, sn);
        free(sp);
    }
    /* styles.xml */
    {
        char *sb = NULL; size_t sn = 0;
        if (b->styles) sb = wubucell_style_render(b->styles, &sn);
        else {
            static const char *def =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                "<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
                "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
                "<borders count=\"1\"><border/></borders>"
                "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
                "<cellXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/></cellXfs></styleSheet>\n";
            sn = strlen(def); sb = strdup(def);
        }
        wubuoxml_add_part(pkg, "xl/styles.xml", sb, sn);
        free(sb);
    }
    /* sharedStrings.xml */
    sst_t sst = {0};
    if (b->use_sst) {
        for (size_t i = 0; i < b->n; i++)
            for (size_t j = 0; j < b->sheets[i].n; j++) {
                const cell_t *c = &b->sheets[i].cells[j];
                if (c->kind == C_STR) cell_sst_add(&sst, c->text ? c->text : "");
            }
        char *sb = NULL; size_t sn = 0; FILE *m = open_memstream(&sb, &sn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"%zu\" uniqueCount=\"%zu\">\n", sst.n, sst.n);
        for (size_t i = 0; i < sst.n; i++) {
            fprintf(m, "<si><t xml:space=\"preserve\">"); cell_xml_escape(m, sst.e[i].s); fprintf(m, "</t></si>\n");
        }
        fprintf(m, "</sst>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "xl/sharedStrings.xml", sb, sn);
        free(sb);
    }
    /* worksheets */
    for (size_t i = 0; i < b->n; i++) {
        char *sxml = cell_render_sheet(b, &b->sheets[i], i, &sst);
        char path[64];
        snprintf(path, sizeof path, "xl/worksheets/sheet%d.xml", (int)(i + 1));
        wubuoxml_add_part(pkg, path, sxml, strlen(sxml));
        free(sxml);
        size_t sheet_charts = 0;
        for (size_t k = 0; k < b->ncharts; k++) if (b->charts[k].sheet == (int)(i + 1)) sheet_charts++;
        if (sheet_charts) {
            char *dr = NULL; size_t dn = 0; FILE *m = open_memstream(&dr, &dn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../drawings/drawing1.xml\"/>\n");
            fprintf(m, "</Relationships>\n");
            fflush(m); fclose(m);
            char rp[80]; snprintf(rp, sizeof rp, "xl/worksheets/_rels/sheet%d.xml.rels", (int)(i + 1));
            wubuoxml_add_part(pkg, rp, dr, dn);
            free(dr);
        }
    }
    /* charts + drawing */
    if (has_charts) {
        for (size_t i = 0; i < b->ncharts; i++) {
            char *cx = cell_render_chart(&b->charts[i], i);
            char path[80]; snprintf(path, sizeof path, "xl/charts/chart%d.xml", (int)(i + 1));
            wubuoxml_add_part(pkg, path, cx, strlen(cx));
            free(cx);
            char *cr = NULL; size_t cn = 0; FILE *m = open_memstream(&cr, &cn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart%d.xml\"/>\n", (int)(i + 1));
            fprintf(m, "</Relationships>\n");
            fflush(m); fclose(m);
            char rp[80]; snprintf(rp, sizeof rp, "xl/charts/_rels/chart%d.xml.rels", (int)(i + 1));
            wubuoxml_add_part(pkg, rp, cr, cn);
            free(cr);
        }
        {
            char *dw = NULL; size_t dn = 0; FILE *m = open_memstream(&dw, &dn);
            fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m, "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
                         "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                         "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
            fprintf(m, "<xdr:twoCellAnchor><xdr:from><xdr:col>4</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>1</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>"
                         "<xdr:to><xdr:col>10</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>16</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>"
                         "<xdr:graphicFrame><xdr:nvGraphicFramePr><xdr:cNvPr id=\"2\" name=\"Chart 1\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
                         "<xdr:graphicFrameLocks noGrp=\"1\"/>"
                         "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
                         "<c:chart xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" r:id=\"rId1\"/>"
                         "</a:graphicData></a:graphic></xdr:graphicFrame>"
                         "<xdr:clientData/></xdr:twoCellAnchor>\n");
            fprintf(m, "</xdr:wsDr>\n");
            fflush(m); fclose(m);
            wubuoxml_add_part(pkg, "xl/drawings/drawing1.xml", dw, dn);
            free(dw);
            char *dr = NULL; size_t dnr = 0; FILE *m2 = open_memstream(&dr, &dnr);
            fprintf(m2, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
            fprintf(m2, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
            fprintf(m2, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>\n");
            fprintf(m2, "</Relationships>\n");
            fflush(m2); fclose(m2);
            wubuoxml_add_part(pkg, "xl/drawings/_rels/drawing1.xml.rels", dr, dnr);
            free(dr);
        }
    }
    int rc = wubuoxml_finalize(pkg);
    for (size_t i = 0; i < sst.n; i++) free(sst.e[i].s);
    free(sst.e);
    fclose(out);
    return rc;
}

void wubucell_free(wubucell_book *b) {
    if (!b) return;
    for (size_t i = 0; i < b->n; i++) {
        free(b->sheets[i].name);
        for (size_t j = 0; j < b->sheets[i].n; j++) { free(b->sheets[i].cells[j].text); free(b->sheets[i].cells[j].formula); }
        free(b->sheets[i].cells);
        free(b->sheets[i].merges);
        free(b->sheets[i].colw);
    }
    for (size_t i = 0; i < b->ncharts; i++) { free(b->charts[i].title); free(b->charts[i].cats); free(b->charts[i].vals); }
    free(b->sheets);
    free(b->charts);
    if (b->styles) wubucell_style_free(b->styles);
    free(b);
}
