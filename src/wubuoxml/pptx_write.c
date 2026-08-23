/* pptx_write.c -- H13: assemble a minimal valid .pptx from the slide model.
 *
 * Emits [Content_Types].xml, _rels, presentation.xml, a slide master, and
 * one slide part with a title placeholder + bullet body -- enough that
 * PowerPoint/LibreOffice/Keynote open it and show the deck. Round-trips
 * with wubuoxml_pptx_text (title/bullets extract back). C11, no deps
 * beyond the wubuoxml package writer. */
#include "pptx_write.h"
#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubuoxml_pptx_write(const char *out, const char *title,
                        const char **bullets, int nbullets){
    if (!out || !title) return -1;
    FILE *f = fopen(out, "wb");
    if (!f) return -1;
    wubuoxml_package *pkg = wubuoxml_create(f);
    if (!pkg){ fclose(f); return -1; }

    /* content types */
    wubuoxml_add_default_type(pkg, "rels",
        "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/ppt/presentation.xml",
        "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml");
    wubuoxml_add_override(pkg, "/ppt/slideMasters/slideMaster1.xml",
        "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml");
    wubuoxml_add_override(pkg, "/ppt/slides/slide1.xml",
        "application/vnd.openxmlformats-officedocument.presentationml.slide+xml");

    /* root rels */
    wubuoxml_add_relationship(pkg, "",
        "ppt/presentation.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* presentation.xml */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
          "<p:sldIdLst><p:sldId id=\"256\" r:id=\"rId1\"/></p:sldIdLst>"
          "<p:sldSz cx=\"9144000\" cy=\"6858000\"/>"
          "</p:presentation>");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/presentation.xml", bb, bn);
        free(bb);
    }
    /* presentation rels */
    wubuoxml_add_relationship(pkg, "ppt/presentation.xml",
        "slides/slide1.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide");
    wubuoxml_add_relationship(pkg, "ppt/presentation.xml",
        "slideMasters/slideMaster1.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster");

    /* slide master (minimal) */
    {
        const char *master =
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<p:sldMaster xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
          "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
          "<p:cSld><p:spTree>"
          "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
          "<p:grpSpPr/>"
          "</p:spTree></p:cSld>"
          "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" "
          "accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
          "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
          "hlink=\"hlink\" folHlink=\"folHlink\"/>"
          "<p:sldLayoutIdLst/>"
          "</p:sldMaster>";
        wubuoxml_add_part(pkg, "ppt/slideMasters/slideMaster1.xml", master, strlen(master));
    }
    wubuoxml_add_relationship(pkg, "ppt/slideMasters/slideMaster1.xml",
        "../slides/slide1.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide");

    /* slide1.xml: title placeholder + body bullets */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
          "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
          "<p:cSld><p:spTree>"
          "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
          "<p:grpSpPr/>");
        /* title shape */
        fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"2\" name=\"Title 1\"/>"
                   "<p:cNvSpPr/><p:nvPr><p:ph type=\"title\"/></p:nvPr></p:nvSpPr>"
                   "<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>");
        for (const char *p2 = title; *p2; p2++){
            if (*p2 == '&') fputs("&amp;", m);
            else if (*p2 == '<') fputs("&lt;", m);
            else if (*p2 == '>') fputs("&gt;", m);
            else fputc(*p2, m);
        }
        fprintf(m, "</a:t></a:r></a:p></p:txBody></p:sp>");
        /* body bullets */
        fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"3\" name=\"Content Placeholder 2\"/>"
                   "<p:cNvSpPr/><p:nvPr><p:ph idx=\"1\"/></p:nvPr></p:nvSpPr>"
                   "<p:txBody><a:bodyPr/><a:lstStyle/>");
        if (nbullets <= 0)
            fprintf(m, "<a:p/><a:endParaRPr lang=\"en-US\"/>");
        else
            for (int i = 0; i < nbullets && bullets[i]; i++){
                fprintf(m, "<a:p><a:pPr lvl=\"%d\"/><a:r><a:t>", i > 0 ? 1 : 0);
                for (const char *p3 = bullets[i]; *p3; p3++){
                    if (*p3 == '&') fputs("&amp;", m);
                    else if (*p3 == '<') fputs("&lt;", m);
                    else if (*p3 == '>') fputs("&gt;", m);
                    else fputc(*p3, m);
                }
                fprintf(m, "</a:t></a:r></a:p>");
            }
        fprintf(m, "</p:txBody></p:sp>"
                   "</p:spTree></p:cSld></p:sld>");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/slides/slide1.xml", bb, bn);
        free(bb);
    }

    int rc = wubuoxml_finalize(pkg);
    fclose(f);
    return rc;
}
