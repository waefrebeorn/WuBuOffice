/* epub.c -- EPUB 3 writer over dm_doc. See epub.h.
 * Clean-room C11 over wubuzip + wubudoc's shared HTML body renderer. */

#include "epub.h"
#include "doc_md.h"
#include "../../src/wubuzip/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- small growable string buffer ---- */
typedef struct { char *s; size_t n, cap; } ebuf;
static void eb_putn(ebuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) { while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 512; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = '\0';
}
static void eb_puts(ebuf *b, const char *s) { eb_putn(b, s, strlen(s)); }
static void eb_esc(ebuf *b, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
            case '&': eb_puts(b, "&amp;"); break;
            case '<': eb_puts(b, "&lt;"); break;
            case '>': eb_puts(b, "&gt;"); break;
            case '"': eb_puts(b, "&quot;"); break;
            default: eb_putn(b, p, 1);
        }
    }
}

static int is_chapter_head(const dm_block *b) {
    if (b->kind != DM_BLOCK_PARA || !b->para.style) return 0;
    return strcmp(b->para.style, "Heading1") == 0 || strcmp(b->para.style, "Title") == 0;
}

/* A chapter is a [start,end) range of blocks and its display title. */
typedef struct { size_t start, end; char *title; } chapter;

int wubudoc_write_epub(const dm_doc *d, const char *path) {
    if (!d) return -1;

    /* ---- split into chapters at Heading1/Title ---- */
    chapter *chs = NULL; size_t nch = 0, cap = 0;
    size_t i = 0;
    /* content before the first heading becomes an untitled lead chapter */
    if (d->n > 0 && !is_chapter_head(&d->blocks[0])) {
        size_t j = 1; while (j < d->n && !is_chapter_head(&d->blocks[j])) j++;
        chs = realloc(chs, (++cap) * sizeof *chs);
        chs[nch++] = (chapter){0, j, strdup("Introduction")};
        i = j;
    }
    for (; i < d->n; ) {
        size_t start = i;
        const char *title = d->blocks[i].para.text ? d->blocks[i].para.text : "Chapter";
        size_t j = i + 1; while (j < d->n && !is_chapter_head(&d->blocks[j])) j++;
        if (nch + 1 > cap) { cap = cap ? cap * 2 : 4; chs = realloc(chs, cap * sizeof *chs); }
        chs[nch++] = (chapter){start, j, strdup(title)};
        i = j;
    }
    if (nch == 0) { /* empty doc: single empty chapter */
        chs = realloc(chs, sizeof *chs);
        chs[nch++] = (chapter){0, 0, strdup("Document")};
    }

    /* ---- open the container ---- */
    FILE *out = fopen(path, "wb");
    if (!out) { for (size_t k = 0; k < nch; k++) free(chs[k].title); free(chs); return -1; }
    wubuzip_writer *z = wubuzip_create(out);
    if (!z) { fclose(out); for (size_t k = 0; k < nch; k++) free(chs[k].title); free(chs); return -1; }

    int rc = 0;
    /* mimetype MUST be first and STORED (uncompressed) */
    const char *MIME = "application/epub+zip";
    rc |= wubuzip_add(z, "mimetype", MIME, (uint32_t)strlen(MIME));

    /* META-INF/container.xml points at the OPF */
    const char *CONTAINER =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
        " <rootfiles>\n"
        "  <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>\n"
        " </rootfiles>\n"
        "</container>\n";
    rc |= wubuzip_add_deflated(z, "META-INF/container.xml", CONTAINER, (uint32_t)strlen(CONTAINER));

    /* ---- one XHTML file per chapter ---- */
    for (size_t c = 0; c < nch; c++) {
        /* shallow sub-document view over the same blocks (no deep copy) */
        dm_doc sub; memset(&sub, 0, sizeof sub);
        sub.blocks = d->blocks + chs[c].start;
        sub.n = chs[c].end - chs[c].start;
        sub.cap = sub.n;
        char *body = wubudoc_render_html_body(&sub, 1);   /* xhtml=1 */

        ebuf x = {0};
        eb_puts(&x,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
            "<head><meta charset=\"utf-8\"/><title>");
        eb_esc(&x, chs[c].title);
        eb_puts(&x, "</title></head>\n<body>\n");
        if (body) eb_puts(&x, body);
        eb_puts(&x, "</body>\n</html>\n");
        free(body);

        char name[64];
        snprintf(name, sizeof name, "OEBPS/ch%zu.xhtml", c + 1);
        rc |= wubuzip_add_deflated(z, name, x.s ? x.s : "", (uint32_t)x.n);
        free(x.s);
    }

    /* ---- OPF package ---- */
    {
        ebuf o = {0};
        eb_puts(&o,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"bookid\">\n"
            " <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
            "  <dc:identifier id=\"bookid\">urn:uuid:wubuoffice-epub-0001</dc:identifier>\n"
            "  <dc:title>");
        eb_esc(&o, chs[0].title ? chs[0].title : "WuBuOffice Document");
        eb_puts(&o, "</dc:title>\n"
            "  <dc:language>en</dc:language>\n"
            "  <meta property=\"dcterms:modified\">2026-01-01T00:00:00Z</meta>\n"
            " </metadata>\n"
            " <manifest>\n"
            "  <item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>\n"
            "  <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>\n");
        for (size_t c = 0; c < nch; c++) {
            char it[128];
            snprintf(it, sizeof it, "  <item id=\"ch%zu\" href=\"ch%zu.xhtml\" media-type=\"application/xhtml+xml\"/>\n", c + 1, c + 1);
            eb_puts(&o, it);
        }
        eb_puts(&o, " </manifest>\n <spine toc=\"ncx\">\n");
        for (size_t c = 0; c < nch; c++) {
            char sp[64];
            snprintf(sp, sizeof sp, "  <itemref idref=\"ch%zu\"/>\n", c + 1);
            eb_puts(&o, sp);
        }
        eb_puts(&o, " </spine>\n</package>\n");
        rc |= wubuzip_add_deflated(z, "OEBPS/content.opf", o.s ? o.s : "", (uint32_t)o.n);
        free(o.s);
    }

    /* ---- EPUB3 nav.xhtml ---- */
    {
        ebuf n = {0};
        eb_puts(&n,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE html>\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
            "<head><meta charset=\"utf-8\"/><title>Contents</title></head>\n"
            "<body>\n<nav epub:type=\"toc\" id=\"toc\"><h1>Contents</h1>\n<ol>\n");
        for (size_t c = 0; c < nch; c++) {
            eb_puts(&n, "<li><a href=\"ch");
            char num[24]; snprintf(num, sizeof num, "%zu", c + 1); eb_puts(&n, num);
            eb_puts(&n, ".xhtml\">");
            eb_esc(&n, chs[c].title);
            eb_puts(&n, "</a></li>\n");
        }
        eb_puts(&n, "</ol>\n</nav>\n</body>\n</html>\n");
        rc |= wubuzip_add_deflated(z, "OEBPS/nav.xhtml", n.s ? n.s : "", (uint32_t)n.n);
        free(n.s);
    }

    /* ---- EPUB2 toc.ncx (reader compatibility) ---- */
    {
        ebuf t = {0};
        eb_puts(&t,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">\n"
            " <head><meta name=\"dtb:uid\" content=\"urn:uuid:wubuoffice-epub-0001\"/></head>\n"
            " <docTitle><text>");
        eb_esc(&t, chs[0].title ? chs[0].title : "WuBuOffice Document");
        eb_puts(&t, "</text></docTitle>\n <navMap>\n");
        for (size_t c = 0; c < nch; c++) {
            char hdr[96];
            snprintf(hdr, sizeof hdr, "  <navPoint id=\"np%zu\" playOrder=\"%zu\"><navLabel><text>", c + 1, c + 1);
            eb_puts(&t, hdr);
            eb_esc(&t, chs[c].title);
            eb_puts(&t, "</text></navLabel><content src=\"ch");
            char num[24]; snprintf(num, sizeof num, "%zu", c + 1); eb_puts(&t, num);
            eb_puts(&t, ".xhtml\"/></navPoint>\n");
        }
        eb_puts(&t, " </navMap>\n</ncx>\n");
        rc |= wubuzip_add_deflated(z, "OEBPS/toc.ncx", t.s ? t.s : "", (uint32_t)t.n);
        free(t.s);
    }

    rc |= wubuzip_finalize(z);
    fclose(out);
    for (size_t k = 0; k < nch; k++) free(chs[k].title);
    free(chs);
    return rc;
}
