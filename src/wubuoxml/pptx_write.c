/* pptx_write.c -- H13/H17: assemble a valid .pptx from the slide model.
 *
 * Emits [Content_Types].xml, rels, presentation.xml (sldIdLst with one
 * entry per slide), a minimal slide master, and one slide part per slide
 * -- PowerPoint/LibreOffice/Keynote open the deck and show every slide.
 * Round-trips with wubuoxml_pptx_text. C11, no deps beyond the wubuoxml
 * package writer. */
#include "pptx_write.h"
#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XML-escape text into an open memstream */
static void xesc(FILE *m, const char *s){
    for (; s && *s; s++){
        if (*s == '&') fputs("&amp;", m);
        else if (*s == '<') fputs("&lt;", m);
        else if (*s == '>') fputs("&gt;", m);
        else fputc(*s, m);
    }
}

/* build one slide part; returns malloc'd XML (caller frees via free+fclose) */
static char *build_slide_xml(const PptxSlide *sl){
    char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
    if (!m) return NULL;
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
      "<p:sld xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
      "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
      "<p:cSld><p:spTree>"
      "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
      "<p:grpSpPr/>");
    fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"2\" name=\"Title 1\"/>"
               "<p:cNvSpPr/><p:nvPr><p:ph type=\"title\"/></p:nvPr></p:nvSpPr>"
               "<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>");
    xesc(m, sl->title ? sl->title : "");
    fprintf(m, "</a:t></a:r></a:p></p:txBody></p:sp>");
    fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"3\" name=\"Content Placeholder 2\"/>"
               "<p:cNvSpPr/><p:nvPr><p:ph idx=\"1\"/></p:nvPr></p:nvSpPr>"
               "<p:txBody><a:bodyPr/><a:lstStyle/>");
    if (sl->nbullets <= 0)
        fprintf(m, "<a:p/>");
    else
        for (int i = 0; i < sl->nbullets && sl->bullets[i]; i++){
            fprintf(m, "<a:p><a:pPr lvl=\"%d\"/><a:r><a:t>", i > 0 ? 1 : 0);
            xesc(m, sl->bullets[i]);
            fprintf(m, "</a:t></a:r></a:p>");
        }
    fprintf(m, "</p:txBody></p:sp>"
               "</p:spTree></p:cSld></p:sld>");
    fflush(m); fclose(m);
    return bb;
}

int wubuoxml_pptx_write_multi(const char *out, const PptxSlide *slides,
                              int nslides){
    if (!out || !slides || nslides < 1) return -1;
    FILE *f = fopen(out, "wb");
    if (!f) return -1;
    wubuoxml_package *pkg = wubuoxml_create(f);
    if (!pkg){ fclose(f); return -1; }

    wubuoxml_add_default_type(pkg, "rels",
        "application/vnd.openxmlformats-package.relationships+xml");
    wubuoxml_add_default_type(pkg, "xml", "application/xml");
    wubuoxml_add_override(pkg, "/ppt/presentation.xml",
        "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml");
    wubuoxml_add_override(pkg, "/ppt/slideMasters/slideMaster1.xml",
        "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml");
    for (int i = 0; i < nslides; i++){
        char part[64], ct[128];
        snprintf(part, sizeof part, "/ppt/slides/slide%d.xml", i + 1);
        snprintf(ct, sizeof ct,
          "application/vnd.openxmlformats-officedocument.presentationml.slide+xml");
        wubuoxml_add_override(pkg, part, ct);
    }

    wubuoxml_add_relationship(pkg, "", "ppt/presentation.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");

    /* presentation.xml: sldIdLst with N entries */
    {
        char *bb = NULL; size_t bn = 0; FILE *m = open_memstream(&bb, &bn);
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
          "<p:sldIdLst>");
        for (int i = 0; i < nslides; i++)
            fprintf(m, "<p:sldId id=\"%d\" r:id=\"rId%d\"/>", 256 + i, i + 2);
        fprintf(m, "</p:sldIdLst><p:sldSz cx=\"9144000\" cy=\"6858000\"/></p:presentation>");
        fflush(m); fclose(m);
        wubuoxml_add_part(pkg, "ppt/presentation.xml", bb, bn);
        free(bb);
    }
    /* presentation rels: rId1 = master, rId2.. slides */
    wubuoxml_add_relationship(pkg, "ppt/presentation.xml",
        "slideMasters/slideMaster1.xml",
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster");
    for (int i = 0; i < nslides; i++){
        char tgt[48];
        snprintf(tgt, sizeof tgt, "slides/slide%d.xml", i + 1);
        wubuoxml_add_relationship(pkg, "ppt/presentation.xml", tgt,
          "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide");
    }

    /* slide master (minimal) */
    {
        const char *master =
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<p:sldMaster xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
          "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
          "<p:cSld><p:spTree>"
          "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
          "<p:grpSpPr/></p:spTree></p:cSld>"
          "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" "
          "accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
          "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
          "hlink=\"hlink\" folHlink=\"folHlink\"/>"
          "<p:sldLayoutIdLst/></p:sldMaster>";
        wubuoxml_add_part(pkg, "ppt/slideMasters/slideMaster1.xml", master, strlen(master));
    }
    /* master -> each slide (layout-free decks tolerate this) */
    for (int i = 0; i < nslides; i++){
        char tgt[48];
        snprintf(tgt, sizeof tgt, "../slides/slide%d.xml", i + 1);
        wubuoxml_add_relationship(pkg, "ppt/slideMasters/slideMaster1.xml", tgt,
          "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide");
    }

    /* slide parts */
    for (int i = 0; i < nslides; i++){
        char *bb = build_slide_xml(&slides[i]);
        if (!bb){ wubuoxml_finalize(pkg); fclose(f); return -1; }
        char part[64];
        snprintf(part, sizeof part, "ppt/slides/slide%d.xml", i + 1);
        wubuoxml_add_part(pkg, part, bb, strlen(bb));
        free(bb);
    }

    int rc = wubuoxml_finalize(pkg);
    fclose(f);
    return rc;
}

int wubuoxml_pptx_write(const char *out, const char *title,
                        const char **bullets, int nbullets){
    PptxSlide s = { title, bullets, nbullets };
    return wubuoxml_pptx_write_multi(out, &s, 1);
}
