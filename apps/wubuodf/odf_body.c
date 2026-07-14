/* odf_body.c -- shared ODF content-body emitters. See odf_body.h.
 * Clean-room C11. Single source of truth for packaged + flat ODF bodies. */

#include "odf_body.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- sbuf ---- */
void odf_sb_putn(odf_sbuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) {
        while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 1024;
        b->s = realloc(b->s, b->cap);
    }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = '\0';
}
void odf_sb_puts(odf_sbuf *b, const char *s) { odf_sb_putn(b, s, strlen(s)); }
void odf_sb_esc(odf_sbuf *b, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
            case '&': odf_sb_puts(b, "&amp;"); break;
            case '<': odf_sb_puts(b, "&lt;"); break;
            case '>': odf_sb_puts(b, "&gt;"); break;
            default: odf_sb_putn(b, p, 1);
        }
    }
}

const char *WUBUODF_NS_ALL =
    "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
    "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
    "xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
    "xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\" "
    "xmlns:presentation=\"urn:oasis:names:tc:opendocument:xmlns:presentation:1.0\" "
    "xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" "
    "xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" "
    "xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" "
    "xmlns:meta=\"urn:oasis:names:tc:opendocument:xmlns:meta:1.0\"";

/* ---- text body (dm_doc) ---- */
static int heading_level(const char *style) {
    if (!style) return 0;
    if (strcmp(style, "Title") == 0 || strcmp(style, "Heading1") == 0) return 1;
    if (strcmp(style, "Heading2") == 0) return 2;
    if (strcmp(style, "Heading3") == 0) return 3;
    return 0;
}

void wubuodf_emit_text_body(odf_sbuf *b, const dm_doc *d) {
    odf_sb_puts(b, "<office:body><office:text>\n");
    for (size_t i = 0; d && i < d->n; i++) {
        const dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            int lvl = heading_level(bl->para.style);
            const char *txt = bl->para.text ? bl->para.text : "";
            if (lvl) {
                char tag[64];
                snprintf(tag, sizeof tag, "<text:h text:outline-level=\"%d\">", lvl);
                odf_sb_puts(b, tag); odf_sb_esc(b, txt); odf_sb_puts(b, "</text:h>\n");
            } else {
                odf_sb_puts(b, "<text:p>");
                if (bl->para.bold) { odf_sb_puts(b, "<text:span text:style-name=\"Bold\">"); odf_sb_esc(b, txt); odf_sb_puts(b, "</text:span>"); }
                else odf_sb_esc(b, txt);
                odf_sb_puts(b, "</text:p>\n");
            }
        } else {
            const dm_table *t = &bl->table;
            odf_sb_puts(b, "<table:table>\n");
            char col[96];
            snprintf(col, sizeof col, "<table:table-column table:number-columns-repeated=\"%zu\"/>\n", t->cols);
            odf_sb_puts(b, col);
            for (size_t r = 0; r < t->rows; r++) {
                odf_sb_puts(b, "<table:table-row>\n");
                for (size_t c = 0; c < t->cols; c++) {
                    dm_para *cell = t->cells[r * t->cols + c];
                    odf_sb_puts(b, "<table:table-cell office:value-type=\"string\"><text:p>");
                    odf_sb_esc(b, (cell && cell->text) ? cell->text : "");
                    odf_sb_puts(b, "</text:p></table:table-cell>\n");
                }
                odf_sb_puts(b, "</table:table-row>\n");
            }
            odf_sb_puts(b, "</table:table>\n");
        }
    }
    odf_sb_puts(b, "</office:text></office:body>\n");
}

/* ---- spreadsheet body (wubucell_book) ---- */
void wubuodf_emit_sheet_body(odf_sbuf *b, const wubucell_book *bk) {
    odf_sb_puts(b, "<office:body><office:spreadsheet>\n");
    int ns = bk ? wubucell_sheet_count(bk) : 0;
    for (int s = 1; s <= ns; s++) {
        odf_sb_puts(b, "<table:table table:name=\"");
        odf_sb_esc(b, wubucell_sheet_name(bk, s));
        odf_sb_puts(b, "\">\n");
        int mc = 0, mr = 0; wubucell_sheet_dims(bk, s, &mc, &mr);
        char col[96];
        snprintf(col, sizeof col, "<table:table-column table:number-columns-repeated=\"%d\"/>\n", mc > 0 ? mc : 1);
        odf_sb_puts(b, col);
        for (int r = 1; r <= mr; r++) {
            odf_sb_puts(b, "<table:table-row>\n");
            for (int c = 1; c <= mc; c++) {
                wubucell_ckind k; const char *t = NULL; double num = 0, cached = 0;
                if (wubucell_get(bk, s, c, r, &k, &t, &num, &cached) != 0) {
                    odf_sb_puts(b, "<table:table-cell/>\n");
                    continue;
                }
                char buf[128];
                if (k == WUBUCELL_STR) {
                    odf_sb_puts(b, "<table:table-cell office:value-type=\"string\"><text:p>");
                    odf_sb_esc(b, t ? t : "");
                    odf_sb_puts(b, "</text:p></table:table-cell>\n");
                } else if (k == WUBUCELL_NUM) {
                    snprintf(buf, sizeof buf, "<table:table-cell office:value-type=\"float\" office:value=\"%g\"><text:p>%g</text:p></table:table-cell>\n", num, num);
                    odf_sb_puts(b, buf);
                } else {
                    odf_sb_puts(b, "<table:table-cell office:value-type=\"float\" table:formula=\"of:=");
                    odf_sb_esc(b, t ? t : "");
                    snprintf(buf, sizeof buf, "\" office:value=\"%g\"><text:p>%g</text:p></table:table-cell>\n", cached, cached);
                    odf_sb_puts(b, buf);
                }
            }
            odf_sb_puts(b, "</table:table-row>\n");
        }
        odf_sb_puts(b, "</table:table>\n");
    }
    odf_sb_puts(b, "</office:spreadsheet></office:body>\n");
}

/* ---- presentation body (wubushow_pres) ---- */
static void emit_pres_lines(odf_sbuf *b, const char *body) {
    const char *p = body ? body : "";
    const char *start = p;
    for (;; p++) {
        if (*p == '\n' || *p == '\0') {
            odf_sb_puts(b, "<text:p>");
            for (const char *q = start; q < p; q++) {
                switch (*q) {
                    case '&': odf_sb_puts(b, "&amp;"); break;
                    case '<': odf_sb_puts(b, "&lt;"); break;
                    case '>': odf_sb_puts(b, "&gt;"); break;
                    default: odf_sb_putn(b, q, 1);
                }
            }
            odf_sb_puts(b, "</text:p>");
            if (*p == '\0') break;
            start = p + 1;
        }
    }
}

void wubuodf_emit_pres_body(odf_sbuf *b, const wubushow_pres *pres) {
    odf_sb_puts(b, "<office:body><office:presentation>\n");
    int ns = pres ? wubushow_slide_count(pres) : 0;
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(pres, i, &title, &body);
        odf_sb_puts(b, "<draw:page draw:name=\"");
        char nm[32]; snprintf(nm, sizeof nm, "Slide%d", i + 1); odf_sb_puts(b, nm);
        odf_sb_puts(b, "\">\n");
        odf_sb_puts(b, "<draw:frame presentation:class=\"title\" "
                    "svg:width=\"20cm\" svg:height=\"3cm\" svg:x=\"2cm\" svg:y=\"1cm\">"
                    "<draw:text-box><text:p>");
        odf_sb_esc(b, title ? title : "");
        odf_sb_puts(b, "</text:p></draw:text-box></draw:frame>\n");
        odf_sb_puts(b, "<draw:frame presentation:class=\"outline\" "
                    "svg:width=\"20cm\" svg:height=\"12cm\" svg:x=\"2cm\" svg:y=\"5cm\">"
                    "<draw:text-box>");
        emit_pres_lines(b, body);
        odf_sb_puts(b, "</draw:text-box></draw:frame>\n");
        odf_sb_puts(b, "</draw:page>\n");
    }
    odf_sb_puts(b, "</office:presentation></office:body>\n");
}
