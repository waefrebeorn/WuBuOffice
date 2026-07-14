/* pdf_text.c -- WinAnsi encoding, Helvetica metrics, and word wrapping for the
 * wubupdf writer. See pdf_internal.h. */

#include "pdf_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Helvetica AFM advance widths (1000-unit em) for ASCII 32..126. This is the
 * canonical Adobe metric set; index = code - 32. */
static const short HELV[95] = {
    278,278,355,556,556,889,667,191,333,333,389,584,278,333,278,278,
    556,556,556,556,556,556,556,556,556,556,278,278,584,584,584,556,
    1015,667,667,722,722,667,611,778,722,278,500,667,556,833,722,778,
    667,778,722,667,611,722,667,944,667,667,611,278,278,278,469,556,
    333,556,556,500,556,556,278,556,556,222,222,500,222,833,556,556,
    556,556,333,500,278,556,500,722,500,500,500,334,260,334,584
};
static const short HELVB[95] = {
    278,333,474,556,556,889,722,238,333,333,389,584,278,333,278,278,
    556,556,556,556,556,556,556,556,556,556,333,333,584,584,584,611,
    975,722,722,722,722,667,611,778,722,278,556,722,611,833,722,778,
    667,778,722,667,611,722,667,944,667,667,611,333,278,333,584,556,
    333,556,611,556,611,556,333,611,611,278,278,556,278,889,611,611,
    611,611,389,556,333,611,556,778,556,556,500,389,280,389,584
};

double pdf_text_width(const char *s, size_t len, double size, int bold) {
    const short *t = bold ? HELVB : HELV;
    double w = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        /* undo the PDF escaping ("\(" "\)" "\\") so widths count one glyph */
        if (ch == '\\' && i + 1 < len) { ch = (unsigned char)s[++i]; }
        int idx = (ch >= 32 && ch <= 126) ? ch - 32 : ('?' - 32);
        w += t[idx];
    }
    return w * size / 1000.0;
}

/* Map a Unicode code point into a WinAnsi byte, or 0 if unrepresentable.
 * Covers Latin-1 (identity 0xA0-0xFF) plus the CP1252 0x80-0x9F specials that
 * our legacy readers emit (smart quotes, dashes, ellipsis, bullet, euro...). */
static int uni_to_winansi(uint32_t cp) {
    if (cp <= 0x7F) return (int)cp;
    if (cp >= 0xA0 && cp <= 0xFF) return (int)cp;
    switch (cp) {
        case 0x20AC: return 0x80; case 0x201A: return 0x82; case 0x0192: return 0x83;
        case 0x201E: return 0x84; case 0x2026: return 0x85; case 0x2020: return 0x86;
        case 0x2021: return 0x87; case 0x02C6: return 0x88; case 0x2030: return 0x89;
        case 0x0160: return 0x8A; case 0x2039: return 0x8B; case 0x0152: return 0x8C;
        case 0x017D: return 0x8E; case 0x2018: return 0x91; case 0x2019: return 0x92;
        case 0x201C: return 0x93; case 0x201D: return 0x94; case 0x2022: return 0x95;
        case 0x2013: return 0x96; case 0x2014: return 0x97; case 0x02DC: return 0x98;
        case 0x2122: return 0x99; case 0x0161: return 0x9A; case 0x203A: return 0x9B;
        case 0x0153: return 0x9C; case 0x017E: return 0x9E; case 0x0178: return 0x9F;
        default: return 0;
    }
}

char *pdf_encode_winansi(const char *utf8) {
    if (!utf8) utf8 = "";
    size_t n = strlen(utf8);
    /* worst case: every byte escaped -> 2x, plus NUL */
    char *out = malloc(n * 2 + 1);
    if (!out) return NULL;
    char *o = out;
    size_t i = 0;
    while (i < n) {
        unsigned char b = (unsigned char)utf8[i];
        uint32_t cp;
        size_t adv;
        if (b < 0x80)          { cp = b; adv = 1; }
        else if ((b & 0xE0) == 0xC0 && i + 1 < n) { cp = ((b & 0x1F) << 6) | (utf8[i+1] & 0x3F); adv = 2; }
        else if ((b & 0xF0) == 0xE0 && i + 2 < n) { cp = ((b & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F); adv = 3; }
        else if ((b & 0xF8) == 0xF0 && i + 3 < n) { cp = 0x3F; adv = 4; }  /* astral -> '?' */
        else { cp = 0x3F; adv = 1; }
        i += adv;

        int wa = uni_to_winansi(cp);
        if (wa == 0) wa = '?';
        if (wa == '(' || wa == ')' || wa == '\\') *o++ = '\\';
        *o++ = (char)wa;
    }
    *o = '\0';
    return out;
}

pdf_page *pdf_new_page(pdf_doc *doc, double *y) {
    if (doc->n + 1 > doc->cap) {
        doc->cap = doc->cap ? doc->cap * 2 : 4;
        doc->pages = realloc(doc->pages, doc->cap * sizeof(*doc->pages));
    }
    pdf_page *pg = &doc->pages[doc->n++];
    pg->lines = NULL; pg->n = 0; pg->cap = 0;
    *y = PDF_TOP_Y;
    return pg;
}

static void page_add(pdf_page *pg, const char *text, size_t len, double size, int bold, double gap) {
    if (pg->n + 1 > pg->cap) {
        pg->cap = pg->cap ? pg->cap * 2 : 16;
        pg->lines = realloc(pg->lines, pg->cap * sizeof(*pg->lines));
    }
    pdf_line *ln = &pg->lines[pg->n++];
    ln->text = malloc(len + 1);
    memcpy(ln->text, text, len); ln->text[len] = '\0';
    ln->size = size; ln->bold = bold; ln->gap_after = gap;
}

void pdf_emit_wrapped(pdf_doc *doc, pdf_page **pg, double *y,
                      const char *s, double size, int bold, double gap_after) {
    double leading = size * 1.35;
    size_t n = strlen(s);

    /* Empty paragraph => a blank line's worth of space. */
    if (n == 0) {
        if (*y - leading < PDF_BOT_Y) { *pg = pdf_new_page(doc, y); }
        *y -= leading + gap_after;
        return;
    }

    size_t start = 0;
    while (start < n) {
        /* find the longest run [start, brk) that fits PDF_TEXT_W */
        size_t brk = start;
        size_t last_space = 0;
        double w = 0;
        size_t i = start;
        while (i < n) {
            /* measure one (possibly escaped) glyph */
            size_t glyph_start = i;
            unsigned char ch = (unsigned char)s[i];
            if (ch == '\\' && i + 1 < n) i++;
            i++;
            double gw = pdf_text_width(s + glyph_start, i - glyph_start, size, bold);
            if (w + gw > PDF_TEXT_W && brk > start) break;
            w += gw;
            /* record a break opportunity at spaces */
            if (s[glyph_start] == ' ') last_space = i;
            brk = i;
        }
        size_t line_end = brk;
        if (brk < n && last_space > start) line_end = last_space; /* break at word */

        /* trim a single trailing space from the emitted slice */
        size_t emit_end = line_end;
        if (emit_end > start && s[emit_end - 1] == ' ') emit_end--;

        if (*y - leading < PDF_BOT_Y) { *pg = pdf_new_page(doc, y); }
        page_add(*pg, s + start, emit_end - start, size, bold, 0);
        *y -= leading;

        start = line_end;
        while (start < n && s[start] == ' ') start++; /* skip leading spaces */
    }
    *y -= gap_after;
}

void pdf_doc_free(pdf_doc *doc) {
    if (!doc) return;
    for (size_t i = 0; i < doc->n; i++) {
        for (size_t j = 0; j < doc->pages[i].n; j++) free(doc->pages[i].lines[j].text);
        free(doc->pages[i].lines);
    }
    free(doc->pages);
    doc->pages = NULL; doc->n = doc->cap = 0;
}
