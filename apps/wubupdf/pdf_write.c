/* pdf_write.c -- lay out a dm_doc into pages and serialize a valid PDF file.
 * See pdf.h / pdf_internal.h. */

#include "pdf.h"
#include "pdf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Map a paragraph style to a font size + bold + spacing. */
static void style_metrics(const char *style, int para_bold,
                          double *size, int *bold, double *gap) {
    *size = 11.0; *bold = para_bold; *gap = 6.0;
    if (!style) return;
    if (strcmp(style, "Title") == 0)         { *size = 24.0; *bold = 1; *gap = 12.0; }
    else if (strcmp(style, "Heading1") == 0) { *size = 18.0; *bold = 1; *gap = 10.0; }
    else if (strcmp(style, "Heading2") == 0) { *size = 15.0; *bold = 1; *gap = 8.0; }
    else if (strcmp(style, "Heading3") == 0) { *size = 13.0; *bold = 1; *gap = 7.0; }
}

/* Lay out the model into pages. */
static void layout(const dm_doc *d, pdf_doc *out) {
    double y = 0;
    pdf_page *pg = pdf_new_page(out, &y);

    for (size_t i = 0; i < d->n; i++) {
        const dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            double size; int bold; double gap;
            style_metrics(b->para.style, b->para.bold, &size, &bold, &gap);
            char *enc = pdf_encode_winansi(b->para.text ? b->para.text : "");
            /* honor embedded newlines: emit each hard line separately */
            char *seg = enc, *nl;
            do {
                nl = strchr(seg, '\n');
                if (nl) *nl = '\0';
                pdf_emit_wrapped(out, &pg, &y, seg, size, bold, nl ? 0 : gap);
                if (nl) seg = nl + 1;
            } while (nl);
            free(enc);
        } else {
            /* table: each row rendered as "a    b    c" (tab-ish spacing) */
            for (size_t r = 0; r < b->table.rows; r++) {
                /* build the joined row text in UTF-8, then encode once */
                size_t cap = 64, len = 0;
                char *row = malloc(cap); row[0] = '\0';
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cc = b->table.cells[r * b->table.cols + c];
                    const char *v = (cc && cc->text) ? cc->text : "";
                    size_t vl = strlen(v);
                    size_t need = len + vl + 4;
                    if (need + 1 > cap) { while (need + 1 > cap) cap *= 2; row = realloc(row, cap); }
                    memcpy(row + len, v, vl); len += vl;
                    if (c + 1 < b->table.cols) { memcpy(row + len, "    ", 4); len += 4; }
                    row[len] = '\0';
                }
                char *enc = pdf_encode_winansi(row);
                int rbold = (r == 0);   /* header row bold */
                pdf_emit_wrapped(out, &pg, &y, enc, 11.0, rbold, 2.0);
                free(enc); free(row);
            }
            /* trailing gap after the table */
            if (y - 8.0 > PDF_BOT_Y) y -= 8.0;
        }
    }
}

/* Build one page's content stream (BT ... ET) as a malloc'd string. */
static char *build_content(const pdf_page *pg, size_t *out_len) {
    size_t cap = 256, len = 0;
    char *s = malloc(cap);
    #define APP(fmt, ...) do { \
        int need = snprintf(NULL, 0, fmt, __VA_ARGS__); \
        if (len + (size_t)need + 1 > cap) { while (len + (size_t)need + 1 > cap) cap *= 2; s = realloc(s, cap); } \
        len += (size_t)snprintf(s + len, cap - len, fmt, __VA_ARGS__); \
    } while (0)

    double y = PDF_TOP_Y;
    for (size_t i = 0; i < pg->n; i++) {
        const pdf_line *ln = &pg->lines[i];
        double leading = ln->size * 1.35;
        y -= leading;
        const char *font = ln->bold ? "F2" : "F1";
        APP("BT /%s %.1f Tf 1 0 0 1 %.1f %.1f Tm (%s) Tj ET\n",
            font, ln->size, PDF_MARGIN, y, ln->text);
        y -= ln->gap_after;
    }
    #undef APP
    *out_len = len;
    return s;
}

int wubupdf_write(const dm_doc *doc, const char *outpath) {
    if (!doc || !outpath) return -1;

    pdf_doc pd = {0};
    layout(doc, &pd);
    if (pd.n == 0) { double y; pdf_new_page(&pd, &y); }  /* always >=1 page */

    FILE *f = fopen(outpath, "wb");
    if (!f) { pdf_doc_free(&pd); return -1; }

    /* Object numbering:
     *   1 = Catalog
     *   2 = Pages
     *   3 = Font Helvetica (F1)
     *   4 = Font Helvetica-Bold (F2)
     *   5..(5+n-1)         = Page objects
     *   (5+n)..(5+2n-1)    = Content streams */
    size_t np = pd.n;
    size_t nobj = 4 + 2 * np;
    long *offset = calloc(nobj + 1, sizeof(long));
    if (!offset) { fclose(f); pdf_doc_free(&pd); return -1; }

    long pos = 0;
    #define OUT(...) do { pos += fprintf(f, __VA_ARGS__); } while (0)
    #define MARK(n)  do { offset[(n)] = pos; } while (0)

    OUT("%%PDF-1.7\n");

    /* 1: Catalog */
    MARK(1); OUT("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    /* 2: Pages */
    MARK(2);
    OUT("2 0 obj\n<< /Type /Pages /Count %zu /Kids [", np);
    for (size_t i = 0; i < np; i++) OUT("%zu 0 R ", 5 + i);
    OUT("] >>\nendobj\n");

    /* 3,4: Fonts */
    MARK(3); OUT("3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
    MARK(4); OUT("4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\nendobj\n");

    /* Page objects + content streams */
    for (size_t i = 0; i < np; i++) {
        size_t page_obj = 5 + i;
        size_t cont_obj = 5 + np + i;
        MARK(page_obj);
        OUT("%zu 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.0f %.0f] "
            "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents %zu 0 R >>\nendobj\n",
            page_obj, PDF_PAGE_W, PDF_PAGE_H, cont_obj);
    }
    for (size_t i = 0; i < np; i++) {
        size_t cont_obj = 5 + np + i;
        size_t clen = 0;
        char *content = build_content(&pd.pages[i], &clen);
        MARK(cont_obj);
        OUT("%zu 0 obj\n<< /Length %zu >>\nstream\n", cont_obj, clen);
        fwrite(content, 1, clen, f); pos += (long)clen;
        OUT("endstream\nendobj\n");
        free(content);
    }

    /* xref */
    long xref_pos = pos;
    OUT("xref\n0 %zu\n", nobj + 1);
    OUT("%010d %05d f \n", 0, 65535);
    for (size_t i = 1; i <= nobj; i++) OUT("%010ld %05d n \n", offset[i], 0);

    OUT("trailer\n<< /Size %zu /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
        nobj + 1, xref_pos);

    #undef OUT
    #undef MARK
    fclose(f);
    free(offset);
    pdf_doc_free(&pd);
    return 0;
}
