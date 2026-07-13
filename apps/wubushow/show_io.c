/* WuBuOffice -- apps/wubushow/show_io
 * Package assembly (wubushow_assemble) and teardown (wubushow_free). Builds the
 * OOXML .pptx package: presentation, slide master/layout, theme and slides.
 * Reuses the slide renderer from show_model.c.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "show_internal.h"
#include "../wubuoxml/package.h"

int wubushow_assemble(wubushow_pres *p, const char *outpath) {
    FILE *out = fopen(outpath, "wb");
    if (!out) { perror("fopen"); return -1; }
    wubuoxml_package *pkg = wubuoxml_create(out);
    wubuoxml_add_default_type(pkg, "rels", "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/ppt/presentation.xml", "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml");
    wubuoxml_add_override(pkg, "/ppt/slideMasters/slideMaster1.xml", "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml");
    wubuoxml_add_override(pkg, "/ppt/slideLayouts/slideLayout1.xml", "application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml");
    wubuoxml_add_override(pkg, "/ppt/theme/theme1.xml", "application/vnd.openxmlformats-officedocument.theme+xml");
    for (size_t i = 0; i < p->n; i++) {
        char path[64];
        snprintf(path, sizeof path, "/ppt/slides/slide%zu.xml", i + 1);
        wubuoxml_add_override(pkg, path, "application/vnd.openxmlformats-officedocument.presentationml.slide+xml");
    }
    /* root rels */
    wubuoxml_add_relationship(pkg, "", "ppt/presentation.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* presentation.xml */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<p:presentation xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">\n");
        fprintf(m, "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst>\n");
        fprintf(m, "<p:sldIdLst>\n");
        for (size_t i = 0; i < p->n; i++)
            fprintf(m, "  <p:sldId id=\"%zu\" r:id=\"rId%zu\"/>\n", 256 + i, i + 2);
        fprintf(m, "</p:sldIdLst>\n");
        fprintf(m, "<p:sldSz cx=\"9144000\" cy=\"6858000\"/><p:notesSz cx=\"6858000\" cy=\"9144000\"/>\n");
        fprintf(m, "</p:presentation>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/presentation.xml", b, n);
        free(b);
    }
    /* presentation rels */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" Target=\"slideMasters/slideMaster1.xml\"/>\n");
        for (size_t i = 0; i < p->n; i++)
            fprintf(m, "  <Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" Target=\"slides/slide%zu.xml\"/>\n",
                    i + 2, i + 1);
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/_rels/presentation.xml.rels", b, n);
        free(b);
    }
    /* slideMaster */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<p:sldMaster xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">\n");
        fprintf(m, "<p:cSld><p:bg><p:bgPr><a:solidFill><a:srgbClr val=\"FFFFFF\"/></a:solidFill></p:bgPr></p:bg>\n");
        fprintf(m, "<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld>\n");
        fprintf(m, "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" hlink=\"hlink\" folHlink=\"folHlink\"/>\n");
        fprintf(m, "<p:sldLayoutIdLst><p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/></p:sldLayoutIdLst>\n");
        fprintf(m, "<p:txStyles><p:titleStyle/><p:bodyStyle/><p:otherStyle/></p:txStyles>\n");
        fprintf(m, "</p:sldMaster>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/slideMasters/slideMaster1.xml", b, n);
        free(b);
    }
    /* slideMaster rels */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>\n");
        fprintf(m, "  <Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" Target=\"../theme/theme1.xml\"/>\n");
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/slideMasters/_rels/slideMaster1.xml.rels", b, n);
        free(b);
    }
    /* slideLayout */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<p:sldLayout xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" type=\"title\"/>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/slideLayouts/slideLayout1.xml", b, n);
        free(b);
    }
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" Target=\"../slideMasters/slideMaster1.xml\"/>\n");
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/slideLayouts/_rels/slideLayout1.xml.rels", b, n);
        free(b);
    }
    /* theme */
    {
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" name=\"Office Theme\"><a:themeElements><a:clrScheme name=\"Office\"><a:dk1><a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1><a:lt1><a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1><a:dk2><a:srgbClr val=\"44546A\"/></a:dk2><a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2><a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1><a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2><a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3><a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4><a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5><a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6><a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink><a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink></a:clrScheme><a:fontScheme name=\"Office\"><a:majorFont><a:latin typeface=\"Calibri Light\"/><a:ea typeface=\"\"/><a:cs typeface=\"\"/></a:majorFont><a:minorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/><a:cs typeface=\"\"/></a:minorFont></a:fontScheme><a:fmtScheme name=\"Office\"><a:fillStyleLst/><a:lnStyleLst/><a:effectStyleLst/><a:bgFillStyleLst/></a:fmtScheme></a:themeElements></a:theme>\n");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/theme/theme1.xml", b, n);
        free(b);
    }
    /* slides + slide rels */
    for (size_t i = 0; i < p->n; i++) {
        char *sxml = show_render_slide(&p->slides[i], (int)(i + 1));
        char path[64];
        snprintf(path, sizeof path, "ppt/slides/slide%zu.xml", i + 1);
        wubuoxml_add_part(pkg, path, sxml, strlen(sxml));
        free(sxml);
        char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        fprintf(m, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
        fprintf(m, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>\n");
        fprintf(m, "</Relationships>\n");
        fflush(m); fclose(m);
        char rpath[80];
        snprintf(rpath, sizeof rpath, "ppt/slides/_rels/slide%zu.xml.rels", i + 1);
        wubuoxml_add_part(pkg, rpath, b, n);
        free(b);
    }
    int rc = wubuoxml_finalize(pkg);
    fclose(out);
    return rc;
}

void wubushow_free(wubushow_pres *p) {
    if (!p) return;
    for (size_t i = 0; i < p->n; i++) { free(p->slides[i].title); free(p->slides[i].body); }
    free(p->slides);
    free(p);
}
