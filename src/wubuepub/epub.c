/* epub.c -- dependency-free C11 EPUB3 writer (see epub.h). */
#include "epub.h"
#include "wubuzip/zip.h"
#include "wububase.h"   /* shared Buf + wububase_xml_escape (was private here) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* heading level from a paragraph's style name ("Heading 1"->1, "Title"->1) */
static int heading_level(const wubumodel_node *para){
    wubumodel_style *st = wubumodel_node_style(para);
    if (!st) return 0;
    const char *name = wubumodel_style_get_prop(st, "name");
    if (!name) return 0;
    if (strstr(name, "Title")) return 1;
    if (strncmp(name, "Heading", 7) == 0){
        int lvl = name[7] - '0';
        if (lvl >= 1 && lvl <= 6) return lvl;
        return 1;
    }
    return 0;
}

static void emit_inline(Buf *s, const wubumodel_node *para){
    for (wubumodel_node *c = wubumodel_node_first_child(para); c; c = wubumodel_node_next_sibling(c)){
        wubumodel_kind k = wubumodel_node_kind(c);
        if (k == WUBUMODEL_RUN){
            const char *t = wubumodel_run_text(c);
            if (t) wububase_xml_escape(s, t);
        } else if (k == WUBUMODEL_LINK){
            wubumodel_style *ls = wubumodel_node_style(c);
            const char *href = ls ? wubumodel_style_get_prop(ls, "href") : NULL;
            const char *lt   = ls ? wubumodel_style_get_prop(ls, "text") : NULL;
            wububase_buf_printf(s, "<a href=\"%s\">", href?href:"#");
            if (lt) wububase_xml_escape(s, lt);
            wububase_buf_add(s, "</a>");
        }
    }
}

static void emit_block(Buf *s, const wubumodel_node *n, int *chap_idx){
    wubumodel_kind k = wubumodel_node_kind(n);
    if (k == WUBUMODEL_SECTION){
        /* a section starts a new chapter */
        (*chap_idx)++;
        Buf chap; wububase_buf_init(&chap);
        wububase_buf_printf(&chap, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<!DOCTYPE html>\n"
                       "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                       "xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
                       "<head><title>Chapter %d</title></head>\n<body>\n", *chap_idx);
        for (wubumodel_node *c = wubumodel_node_first_child(n); c; c = wubumodel_node_next_sibling(c))
            emit_block(&chap, c, chap_idx);
        wububase_buf_add(&chap, "</body>\n</html>\n");
        /* caller assembles chapters list; here we emit inline into s (single doc) */
        wububase_buf_add(s, wububase_buf_str(&chap));
        wububase_buf_free(&chap);
    } else if (k == WUBUMODEL_PARAGRAPH){
        int lvl = heading_level(n);
        if (lvl){
            wububase_buf_printf(s, "<h%d>", lvl);
            emit_inline(s, n);
            wububase_buf_printf(s, "</h%d>\n", lvl);
        } else {
            wububase_buf_add(s, "<p>");
            emit_inline(s, n);
            wububase_buf_add(s, "</p>\n");
        }
    } else if (k == WUBUMODEL_TABLE){
        wububase_buf_add(s, "<table>\n");
        for (wubumodel_node *r = wubumodel_node_first_child(n); r; r = wubumodel_node_next_sibling(r)){
            wububase_buf_add(s, "<tr>\n");
            for (wubumodel_node *cell = wubumodel_node_first_child(r); cell; cell = wubumodel_node_next_sibling(cell)){
                wububase_buf_add(s, "<td>");
                emit_inline(s, cell);
                wububase_buf_add(s, "</td>\n");
            }
            wububase_buf_add(s, "</tr>\n");
        }
        wububase_buf_add(s, "</table>\n");
    }
    /* SHAPE/CHART: skip in v1 (draw/math can be rasterized later) */
}

int epub_write(const wubumodel_doc *doc, const char *path,
               const char *title, const char *lang){
    if (!doc || !path) return -1;
    const char *t = title && *title ? title : "Untitled";
    const char *lg = lang && *lang ? lang : "en";

    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    wubuzip_writer *z = wubuzip_create(fp);
    if (!z){ fclose(fp); return -1; }

    /* 1) mimetype MUST be first and STORED (uncompressed) */
    const char *mime = "application/epub+zip";
    if (wubuzip_add(z, "mimetype", mime, (uint32_t)strlen(mime)) != 0) goto fail;

    /* 2) META-INF/container.xml */
    const char *container =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
        "  <rootfiles>\n"
        "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
        "  </rootfiles>\n"
        "</container>\n";
    if (wubuzip_add_deflated(z, "META-INF/container.xml", container, (uint32_t)strlen(container)) != 0) goto fail;

    /* 3) count chapters + build body + nav */
    Buf body; wububase_buf_init(&body);
    int chap = 0;
    wubumodel_node *root = wubumodel_doc_root(doc);
    /* gather top-level sections; if none, wrap whole doc as one chapter */
    int nsec = 0;
    for (wubumodel_node *c = root; c; c = wubumodel_node_next_sibling(c))
        if (wubumodel_node_kind(c) == WUBUMODEL_SECTION) nsec++;
    if (nsec == 0){
        wububase_buf_add(&body, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<!DOCTYPE html>\n"
                     "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                     "xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
                     "<head><title>");
        wububase_xml_escape(&body, t); wububase_buf_add(&body, "</title></head>\n<body>\n");
        for (wubumodel_node *c = root; c; c = wubumodel_node_next_sibling(c))
            emit_block(&body, c, &chap);
        wububase_buf_add(&body, "</body>\n</html>\n");
    } else {
        wububase_buf_add(&body, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<!DOCTYPE html>\n"
                     "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                     "xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
                     "<head><title>");
        wububase_xml_escape(&body, t); wububase_buf_add(&body, "</title></head>\n<body>\n");
        for (wubumodel_node *c = root; c; c = wubumodel_node_next_sibling(c))
            if (wubumodel_node_kind(c) == WUBUMODEL_SECTION)
                emit_block(&body, c, &chap);
        wububase_buf_add(&body, "</body>\n</html>\n");
    }

    /* 4) OPF */
    Buf opf; wububase_buf_init(&opf);
    wububase_buf_printf(&opf,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"bookid\">\n"
        "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "    <dc:identifier id=\"bookid\">urn:uuid:wubuoffice-0001</dc:identifier>\n"
        "    <dc:title>%s</dc:title>\n"
        "    <dc:language>%s</dc:language>\n"
        "    <meta property=\"dcterms:modified\">2026-01-01T00:00:00Z</meta>\n"
        "  </metadata>\n"
        "  <manifest>\n"
        "    <item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
        "    <item id=\"chap1\" href=\"chap_1.xhtml\" media-type=\"application/xhtml+xml\"/>\n"
        "  </manifest>\n"
        "  <spine>\n"
        "    <itemref idref=\"chap1\"/>\n"
        "  </spine>\n"
        "</package>\n", t, lg);
    if (wubuzip_add_deflated(z, "OEBPS/content.opf", wububase_buf_str(&opf), (uint32_t)wububase_buf_len(&opf)) != 0){ wububase_buf_free(&opf); wububase_buf_free(&body); goto fail; }
    wububase_buf_free(&opf);

    /* 5) nav.xhtml */
    Buf nav; wububase_buf_init(&nav);
    wububase_buf_printf(&nav,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
        "<head><title>Table of Contents</title></head>\n<body>\n<nav epub:type=\"toc\" id=\"toc\">\n<ol>\n"
        "<li><a href=\"chap_1.xhtml\">%s</a></li>\n"
        "</ol>\n</nav>\n</body>\n</html>\n", t);
    if (wubuzip_add_deflated(z, "OEBPS/nav.xhtml", wububase_buf_str(&nav), (uint32_t)wububase_buf_len(&nav)) != 0){ wububase_buf_free(&nav); wububase_buf_free(&body); goto fail; }
    wububase_buf_free(&nav);

    /* 6) chapter content */
    if (wubuzip_add_deflated(z, "OEBPS/chap_1.xhtml", wububase_buf_str(&body), (uint32_t)wububase_buf_len(&body)) != 0){ wububase_buf_free(&body); goto fail; }
    wububase_buf_free(&body);

    if (wubuzip_finalize(z) != 0) return -1;
    return 0;
fail:
    wubuzip_finalize(z); /* still closes fp */
    return -1;
}
