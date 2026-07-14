/* model_json.c -- JSON serializers for the three WuBuOffice models.
 * See model_json.h. Clean-room C11, no third-party code. */

#include "model_json.h"

#include <stdio.h>
#include <string.h>

/* Write a JSON string literal (quotes + escaping) for `s`. */
static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 0x20) fprintf(f, "\\u%04x", *p);
                else fputc(*p, f);   /* pass UTF-8 bytes through verbatim */
        }
    }
    fputc('"', f);
}

/* ---- document ---- */

int wubudoc_write_doc_json(const dm_doc *d, const char *path) {
    if (!d) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fputs("{\n  \"type\": \"document\",\n  \"blocks\": [\n", f);
    for (size_t i = 0; i < d->n; i++) {
        const dm_block *b = &d->blocks[i];
        fputs("    {", f);
        if (b->kind == DM_BLOCK_PARA) {
            fputs("\"kind\": \"paragraph\", \"style\": ", f);
            if (b->para.style) json_str(f, b->para.style); else fputs("null", f);
            fprintf(f, ", \"bold\": %s, \"text\": ", b->para.bold ? "true" : "false");
            json_str(f, b->para.text);
        } else {
            const dm_table *t = &b->table;
            fprintf(f, "\"kind\": \"table\", \"rows\": %zu, \"cols\": %zu, \"cells\": [",
                    t->rows, t->cols);
            for (size_t r = 0; r < t->rows; r++) {
                fputc('[', f);
                for (size_t c = 0; c < t->cols; c++) {
                    dm_para *cell = t->cells[r * t->cols + c];
                    json_str(f, (cell && cell->text) ? cell->text : "");
                    if (c + 1 < t->cols) fputs(", ", f);
                }
                fputc(']', f);
                if (r + 1 < t->rows) fputs(", ", f);
            }
            fputc(']', f);
        }
        fputs(i + 1 < d->n ? "},\n" : "}\n", f);
    }
    fputs("  ]\n}\n", f);
    fclose(f);
    return 0;
}

/* ---- workbook ---- */

int wubudoc_write_book_json(const wubucell_book *b, const char *path) {
    if (!b) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int ns = wubucell_sheet_count(b);
    fputs("{\n  \"type\": \"workbook\",\n  \"sheets\": [\n", f);
    for (int s = 1; s <= ns; s++) {
        fputs("    {\"name\": ", f);
        json_str(f, wubucell_sheet_name(b, s));
        int mc = 0, mr = 0; wubucell_sheet_dims(b, s, &mc, &mr);
        fprintf(f, ", \"rows\": %d, \"cols\": %d, \"cells\": [", mr, mc);
        int first = 1;
        for (int r = 1; r <= mr; r++) {
            for (int c = 1; c <= mc; c++) {
                wubucell_ckind k; const char *t = NULL; double n = 0, ca = 0;
                if (wubucell_get(b, s, c, r, &k, &t, &n, &ca) != 0) continue;
                if (!first) fputs(", ", f);
                first = 0;
                fprintf(f, "{\"r\": %d, \"c\": %d, ", r, c);
                if (k == WUBUCELL_STR) { fputs("\"kind\": \"str\", \"value\": ", f); json_str(f, t); }
                else if (k == WUBUCELL_NUM) fprintf(f, "\"kind\": \"num\", \"value\": %g", n);
                else { fputs("\"kind\": \"formula\", \"value\": ", f); json_str(f, t); fprintf(f, ", \"cached\": %g", ca); }
                fputc('}', f);
            }
        }
        fputs("]}", f);
        fputs(s < ns ? ",\n" : "\n", f);
    }
    fputs("  ]\n}\n", f);
    fclose(f);
    return 0;
}

/* ---- presentation ---- */

int wubudoc_write_pres_json(const wubushow_pres *p, const char *path) {
    if (!p) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int ns = wubushow_slide_count(p);
    fputs("{\n  \"type\": \"presentation\",\n  \"slides\": [\n", f);
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(p, i, &title, &body);
        fputs("    {\"title\": ", f);
        json_str(f, title);
        fputs(", \"body\": ", f);
        json_str(f, body);
        fputc('}', f);
        fputs(i + 1 < ns ? ",\n" : "\n", f);
    }
    fputs("  ]\n}\n", f);
    fclose(f);
    return 0;
}
