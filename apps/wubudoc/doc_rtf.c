/* doc_rtf.c -- Rich Text Format (RTF 1.x) writer. See doc_rtf.h.
 * Clean-room C11, no third-party code. */

#include "doc_rtf.h"

#include <stdio.h>
#include <string.h>

/* Emit `s` with RTF escaping: backslash, braces, and non-ASCII -> \'hh or \uN.
 * We use \uN escapes (with an ASCII '?' fallback char) so UTF-8 text (e.g. the
 * U+2022 bullet) survives into RTF readers. */
static void rtf_text(FILE *f, const char *s) {
    if (!s) return;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        if (c == '\\' || c == '{' || c == '}') { fputc('\\', f); fputc(c, f); p++; }
        else if (c < 0x80) { fputc(c, f); p++; }
        else {
            /* decode one UTF-8 code point -> \uN? */
            unsigned cp = 0; int n = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 3; }
            else { p++; continue; }
            p++;
            for (int i = 0; i < n && (*p & 0xC0) == 0x80; i++) { cp = (cp << 6) | (*p & 0x3F); p++; }
            /* RTF \uN uses a SIGNED 16-bit value; emit as signed decimal. */
            int sv = (cp > 32767) ? (int)cp - 65536 : (int)cp;
            fprintf(f, "\\u%d?", sv);
        }
    }
}

static void emit_para(FILE *f, const dm_para *pa) {
    const char *style = pa->style;
    int hsize = 0;      /* half-points of font size bump for headings */
    if (style) {
        if (strcmp(style, "Heading1") == 0 || strcmp(style, "Title") == 0) hsize = 36; /* 18pt */
        else if (strcmp(style, "Heading2") == 0) hsize = 32; /* 16pt */
        else if (strcmp(style, "Heading3") == 0) hsize = 28; /* 14pt */
    }
    fputs("\\pard", f);
    if (hsize) fprintf(f, "\\sb120\\sa60\\fs%d\\b", hsize);
    else if (pa->bold) fputs("\\b", f);
    fputc(' ', f);
    rtf_text(f, pa->text ? pa->text : "");
    if (hsize || pa->bold) fputs("\\b0", f);
    fputs("\\par\n", f);
}

int wubudoc_write_rtf(const dm_doc *d, const char *path) {
    if (!d) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    /* header: RTF 1, ANSI, one font (Times) */
    fputs("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Times New Roman;}}\n", f);

    for (size_t i = 0; i < d->n; i++) {
        const dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            emit_para(f, &b->para);
        } else {
            const dm_table *t = &b->table;
            /* Simple fixed-width columns: divide 9000 twips across cols. */
            int colw = t->cols ? (int)(9000 / t->cols) : 9000;
            for (size_t r = 0; r < t->rows; r++) {
                fputs("\\trowd\\trgaph108", f);
                for (size_t c = 0; c < t->cols; c++)
                    fprintf(f, "\\cellx%d", (int)((c + 1) * colw));
                fputc('\n', f);
                for (size_t c = 0; c < t->cols; c++) {
                    dm_para *cell = t->cells[r * t->cols + c];
                    int bold = (r == 0) || (cell && cell->bold);
                    fputs("\\pard\\intbl", f);
                    if (bold) fputs("\\b", f);
                    fputc(' ', f);
                    rtf_text(f, (cell && cell->text) ? cell->text : "");
                    if (bold) fputs("\\b0", f);
                    fputs("\\cell\n", f);
                }
                fputs("\\row\n", f);
            }
        }
    }

    fputs("}\n", f);
    fclose(f);
    return 0;
}
