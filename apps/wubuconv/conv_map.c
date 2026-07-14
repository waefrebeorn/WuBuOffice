/* conv_map.c -- unified format conversion. See conv_map.h.
 * Clean-room C11. Bridges every supported format through three models:
 *   TEXT  : dm_doc        (docx / md / rtf / html / odt)
 *   SHEET : wubucell_book (xlsx / csv / tsv / ods)
 *   SHOW  : wubushow_pres (pptx / odp)
 * CSV/JSON are SHEET/TEXT/SHOW dumps that can target any model.
 *
 * Reader functions build the model; writer functions emit a format. The
 * converter picks a reader by the input extension and a writer by the output
 * extension and, when families differ, bridges through the natural mapping
 * (sheet -> doc as one table; show -> doc as title + bullet body). */

#include "conv_map.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../wubuedit/docmodel.h"
#include "../wubucell/cell.h"
#include "../wubucell/cell_read.h"
#include "../wubucell/cell_csv.h"
#include "../wubushow/show.h"
#include "../wubushow/show_read.h"
#include "../wubuword/word.h"
#include "../wubuword/assemble.h"
#include "../wubudoc/doc_md.h"
#include "../wubudoc/doc_rtf.h"
#include "../wubudoc/model_json.h"
#include "../wubudoc/epub.h"
#include "../wubuodf/odf.h"
#include "../wubulegacy/legacy.h"
#include "../wubupdf/pdf.h"
#include "../wubuoxml/reader.h"
#include "../wubuxml/parser.h"

static char *extof(const char *path) {
    const char *dot = strrchr(path, '.');
    return strdup(dot ? dot + 1 : "");
}
static int ext_is(const char *ext, const char *w) { return strcasecmp(ext, w) == 0; }

static uint8_t *slurp(const char *p, size_t *o) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(s ? (size_t)s : 1);
    if (fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f); *o = (size_t)s; return d;
}

/* forward declarations for dm_doc writers defined later */
static int dm_doc_write_docx(const dm_doc *d, const char *out);
static int dm_doc_write_md(const dm_doc *d, const char *out);
static int dm_doc_write_html(const dm_doc *d, const char *out);

/* ---------- TEXT model (dm_doc) ---------- */

/* read a .md into a dm_doc via the proven md->docx->model pivot */
static int read_md_text(const char *in, dm_doc *out) {
    wubuword_doc *wd = NULL;
    if (wubudoc_read_md(in, &wd) != 0) return -1;
    size_t len = 0; char *xml = wubuword_render(wd, &len);
    wubuword_free(wd);
    int rc = wubuedit_docmodel_parse((const uint8_t *)xml, len, out);
    free(xml);
    return rc;
}

static int read_text(const char *in, const char *ext, dm_doc *out) {
    if (ext_is(ext, "docx")) {
        size_t sz = 0; uint8_t *d = slurp(in, &sz); if (!d) return -1;
        wubuoxml_package pkg; if (wubuoxml_read(d, sz, &pkg) != 0) { free(d); return -1; }
        const wubuoxml_part *dp = wubuoxml_part_find(&pkg, "word/document.xml");
        int rc = dp ? wubuedit_docmodel_parse(dp->bytes, dp->len, out) : -1;
        wubuoxml_free(&pkg); free(d); return rc;
    }
    if (ext_is(ext, "md"))  return read_md_text(in, out);
    if (ext_is(ext, "odt")) return wubuodf_read_odt(in, out);
    if (ext_is(ext, "fodt")) return wubuodf_read_fodt(in, out);
    if (ext_is(ext, "doc")) return wubulegacy_read_doc(in, out);
    return -1;
}

/* ---------- SHEET model (wubucell_book) ---------- */

static int read_sheet(const char *in, const char *ext, wubucell_book **out) {
    if (ext_is(ext, "xlsx")) return wubucell_read(in, out);
    if (ext_is(ext, "csv") || ext_is(ext, "tsv")) {
        char delim = ext_is(ext, "tsv") ? '\t' : ',';
        return wubucell_read_csv(in, delim, out);
    }
    if (ext_is(ext, "ods"))  return wubuodf_read_ods(in, out);
    if (ext_is(ext, "fods")) return wubuodf_read_fods(in, out);
    if (ext_is(ext, "xls"))  return wubulegacy_read_xls(in, out);
    return -1;
}

/* ---------- SHOW model (wubushow_pres) ---------- */

static int read_show(const char *in, const char *ext, wubushow_pres **out) {
    if (ext_is(ext, "pptx")) return wubushow_read(in, out);
    if (ext_is(ext, "odp"))  return wubuodf_read_odp(in, out);
    if (ext_is(ext, "fodp")) return wubuodf_read_fodp(in, out);
    if (ext_is(ext, "ppt"))  return wubulegacy_read_ppt(in, out);
    return -1;
}

/* ============================================================
 *  WRITE side: model -> out_path
 * ============================================================ */

/* TEXT writers */
static int write_text(const char *out, const char *ext, dm_doc *d) {
    if (ext_is(ext, "docx")) return dm_doc_write_docx(d, out);
    if (ext_is(ext, "md"))   return dm_doc_write_md(d, out);
    if (ext_is(ext, "html")) return dm_doc_write_html(d, out);
    if (ext_is(ext, "rtf"))  return wubudoc_write_rtf(d, out);
    if (ext_is(ext, "odt"))  return wubuodf_write_odt(d, out);
    if (ext_is(ext, "fodt")) return wubuodf_write_fodt(d, out);
    if (ext_is(ext, "pdf"))  return wubupdf_write(d, out);
    if (ext_is(ext, "epub")) return wubudoc_write_epub(d, out);
    if (ext_is(ext, "json")) return wubudoc_write_doc_json(d, out);
    return -1;
}

/* SHEET writers */
static int write_sheet(const char *out, const char *ext, wubucell_book *b) {
    if (ext_is(ext, "xlsx")) return wubucell_assemble(b, out);
    if (ext_is(ext, "csv") || ext_is(ext, "tsv")) {
        char delim = ext_is(ext, "tsv") ? '\t' : ',';
        return wubucell_write_csv(b, 1, delim, out);
    }
    if (ext_is(ext, "ods"))  return wubuodf_write_ods(b, out);
    if (ext_is(ext, "fods")) return wubuodf_write_fods(b, out);
    if (ext_is(ext, "json")) return wubudoc_write_book_json(b, out);
    return -1;
}

/* SHOW writers */
static int write_show(const char *out, const char *ext, wubushow_pres *p) {
    if (ext_is(ext, "pptx")) return wubushow_assemble(p, out);
    if (ext_is(ext, "odp"))  return wubuodf_write_odp(p, out);
    if (ext_is(ext, "fodp")) return wubuodf_write_fodp(p, out);
    if (ext_is(ext, "json")) return wubudoc_write_pres_json(p, out);
    return -1;
}

/* ---------- bridges between families ---------- */

/* text model helpers: grow the block array as needed */
#define DM_PUSH(d, blk) do { \
    if ((d)->n + 1 > (d)->cap) { (d)->cap = (d)->cap ? (d)->cap * 2 : 8; (d)->blocks = realloc((d)->blocks, (d)->cap * sizeof(*(d)->blocks)); } \
    (blk) = &((d)->blocks[(d)->n++]); memset((blk), 0, sizeof(*(blk))); \
} while (0)

/* sheet -> text: one big table, header row bold */
static void sheet_to_text(const wubucell_book *b, dm_doc *d) {
    memset(d, 0, sizeof *d);
    d->cap = 8; d->blocks = calloc(d->cap, sizeof *d->blocks);
    int ns = wubucell_sheet_count(b);
    for (int s = 1; s <= ns; s++) {
        int mc = 0, mr = 0; wubucell_sheet_dims(b, s, &mc, &mr);
        char title[256]; snprintf(title, sizeof title, "Sheet: %s", wubucell_sheet_name(b, s));
        dm_block *h; DM_PUSH(d, h); h->kind = DM_BLOCK_PARA; h->para.style = strdup("Heading1"); h->para.text = strdup(title);
        dm_block *t; DM_PUSH(d, t); t->kind = DM_BLOCK_TABLE; t->table.rows = mr; t->table.cols = mc;
        size_t ncells = (size_t)mr * mc; if (ncells == 0) ncells = 1;
        t->table.cells = calloc(ncells, sizeof(dm_para *));
        for (int r = 1; r <= mr; r++) {
            for (int c = 1; c <= mc; c++) {
                wubucell_ckind k; const char *txt = NULL; double num = 0, cached = 0;
                char buf[64];
                if (wubucell_get(b, s, c, r, &k, &txt, &num, &cached) == 0) {
                    const char *val = (k == WUBUCELL_STR) ? txt : buf;
                    if (k != WUBUCELL_STR) snprintf(buf, sizeof buf, "%g", (k == WUBUCELL_NUM) ? num : cached);
                    dm_para *cp = calloc(1, sizeof *cp);
                    cp->text = strdup(val ? val : "");
                    cp->bold = (r == 1);
                    t->table.cells[(size_t)(r - 1) * mc + (c - 1)] = cp;
                }
            }
        }
    }
}

/* show -> text: per-slide title (heading) + body lines as a paragraph */
static void show_to_text(const wubushow_pres *p, dm_doc *d) {
    memset(d, 0, sizeof *d);
    d->cap = 8; d->blocks = calloc(d->cap, sizeof *d->blocks);
    int ns = wubushow_slide_count(p);
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(p, i, &title, &body);
        dm_block *h; DM_PUSH(d, h); h->kind = DM_BLOCK_PARA; h->para.style = strdup("Heading1"); h->para.text = strdup(title ? title : "");
        if (body && body[0]) {
            dm_block *pa; DM_PUSH(d, pa); pa->kind = DM_BLOCK_PARA; pa->para.text = strdup(body);
        }
    }
}

/* text -> sheet: flatten paragraphs as rows, table cells into columns */
static void text_to_sheet(const dm_doc *d, wubucell_book *b) {
    int sh = wubucell_sheet(b, "Sheet1");
    int r = 0;
    for (size_t i = 0; i < d->n; i++) {
        dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            r++;
            wubucell_cell_s(b, sh, 1, r, bl->para.text ? bl->para.text : "");
        } else {
            for (size_t tr = 0; tr < bl->table.rows; tr++) {
                r++;
                for (size_t c = 0; c < bl->table.cols; c++) {
                    size_t idx = tr * bl->table.cols + c;
                    dm_para *cc = (idx < bl->table.rows * bl->table.cols) ? bl->table.cells[idx] : NULL;
                    const char *v = (cc && cc->text) ? cc->text : "";
                    wubucell_cell_s(b, sh, (int)c + 1, r, v);
                }
            }
        }
    }
}

/* text -> show: each heading starts a slide, following paragraphs are body */
static void text_to_show(const dm_doc *d, wubushow_pres *p) {
    char *title = strdup("Untitled");
    /* growable body buffer (no manual realloc aliasing) */
    char *body = NULL; size_t bcap = 0, bcur = 0;
    #define BODY_APPEND(src, len) do { \
        size_t need = bcur + (len); \
        if (need + 1 > bcap) { size_t nc = bcap ? bcap * 2 : 64; while (need + 1 > nc) nc *= 2; body = realloc(body, nc); bcap = nc; } \
        memcpy(body + bcur, (src), (len)); bcur += (len); body[bcur] = '\0'; \
    } while (0)
    for (size_t i = 0; i < d->n; i++) {
        dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            const char *txt = bl->para.text ? bl->para.text : "";
            int is_h = bl->para.style && (strncmp(bl->para.style, "Heading", 7) == 0 || strcmp(bl->para.style, "Title") == 0);
            if (is_h) {
                if (title) { wubushow_slide(p, title, body ? body : ""); }
                free(title); title = strdup(txt);
                free(body); body = NULL; bcur = 0; bcap = 0;
            } else {
                size_t L = strlen(txt);
                if (L) {
                    if (bcur) BODY_APPEND("\n", 1);
                    BODY_APPEND(txt, L);
                }
            }
        } else {
            /* table: emit as body lines */
            for (size_t r = 0; r < bl->table.rows; r++) {
                for (size_t c = 0; c < bl->table.cols; c++) {
                    dm_para *cc = bl->table.cells[r * bl->table.cols + c];
                    const char *v = (cc && cc->text) ? cc->text : "";
                    size_t L = strlen(v);
                    if (bcur) BODY_APPEND(" ", 1);
                    BODY_APPEND(v, L);
                }
                BODY_APPEND("\n", 1);
            }
        }
    }
    #undef BODY_APPEND
    if (title) { wubushow_slide(p, title, body ? body : ""); }
    free(title); free(body);
}

/* ---------- docx renderer for dm_doc ---------- */
static int dm_doc_write_docx(const dm_doc *d, const char *out) {
    wubuword_doc *wd = wubuword_create();
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            wubuword_para(wd, b->para.style, b->para.bold, b->para.text);
        } else {
            wubuword_table_begin(wd);
            for (size_t r = 0; r < b->table.rows; r++) {
                wubuword_row(wd);
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cc = b->table.cells[r * b->table.cols + c];
                    wubuword_cell(wd, cc ? cc->bold : 0, cc ? cc->text : "");
                }
            }
            wubuword_table_end(wd);
        }
    }
    size_t len = 0; char *xml = wubuword_render(wd, &len);
    int rc = wubuword_assemble(out, xml, len);
    free(xml); wubuword_free(wd);
    return rc;
}

/* ---------- md / html writers for dm_doc ---------- */
static int dm_doc_write_md(const dm_doc *d, const char *out) {
    FILE *f = fopen(out, "wb"); if (!f) return -1;
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            int lvl = 0;
            if (b->para.style) {
                if (strcmp(b->para.style, "Title") == 0 || strcmp(b->para.style, "Heading1") == 0) lvl = 1;
                else if (strcmp(b->para.style, "Heading2") == 0) lvl = 2;
                else if (strcmp(b->para.style, "Heading3") == 0) lvl = 3;
            }
            if (lvl) { for (int k = 0; k < lvl; k++) fputc('#', f); fputc(' ', f); }
            if (b->para.bold) fputs("**", f);
            fputs(b->para.text ? b->para.text : "", f);
            if (b->para.bold) fputs("**", f);
            fputc('\n', f); fputc('\n', f);
        } else {
            /* table */
            fputs("| ", f);
            for (size_t c = 0; c < b->table.cols; c++) {
                dm_para *cc = b->table.cells[c];
                fputs(cc && cc->text ? cc->text : "", f);
                fputs(c + 1 < b->table.cols ? " | " : " |\n", f);
            }
            fputs("| ", f);
            for (size_t c = 0; c < b->table.cols; c++)
                fputs(c + 1 < b->table.cols ? "--- | " : "--- |\n", f);
            for (size_t r = 1; r < b->table.rows; r++) {
                fputs("| ", f);
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cc = b->table.cells[r * b->table.cols + c];
                    fputs(cc && cc->text ? cc->text : "", f);
                    fputs(c + 1 < b->table.cols ? " | " : " |\n", f);
                }
            }
            fputc('\n', f);
        }
    }
    fclose(f);
    return 0;
}

static int dm_doc_write_html(const dm_doc *d, const char *out) {
    FILE *f = fopen(out, "wb"); if (!f) return -1;
    fputs("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>WuBuOffice</title></head><body>\n", f);
    for (size_t i = 0; i < d->n; i++) {
        dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            int lvl = 0;
            if (b->para.style) {
                if (strcmp(b->para.style, "Title") == 0 || strcmp(b->para.style, "Heading1") == 0) lvl = 1;
                else if (strcmp(b->para.style, "Heading2") == 0) lvl = 2;
                else if (strcmp(b->para.style, "Heading3") == 0) lvl = 3;
            }
            if (lvl) {
                fprintf(f, "<h%d>", lvl);
                if (b->para.bold) fputs("<strong>", f);
                fputs(b->para.text ? b->para.text : "", f);
                if (b->para.bold) fputs("</strong>", f);
                fprintf(f, "</h%d>\n", lvl);
            } else {
                fputs("<p>", f);
                if (b->para.bold) fputs("<strong>", f);
                fputs(b->para.text ? b->para.text : "", f);
                if (b->para.bold) fputs("</strong>", f);
                fputs("</p>\n", f);
            }
        } else {
            fputs("<table>\n", f);
            for (size_t r = 0; r < b->table.rows; r++) {
                fputs("  <tr>", f);
                for (size_t c = 0; c < b->table.cols; c++) {
                    dm_para *cc = b->table.cells[r * b->table.cols + c];
                    const char *v = (cc && cc->text) ? cc->text : "";
                    if (r == 0) fprintf(f, "<th>%s</th>", v);
                    else fprintf(f, "<td>%s</td>", v);
                }
                fputs("</tr>\n", f);
            }
            fputs("</table>\n", f);
        }
    }
    fputs("</body></html>\n", f);
    fclose(f);
    return 0;
}

/* ============================================================
 *  Top-level convert
 * ============================================================ */

int wubuconv_convert(const char *in_path, const char *out_path) {
    char *inext = extof(in_path);
    char *outext = extof(out_path);

    /* classify families */
    enum { NONE, TEXT, SHEET, SHOW } infam = NONE, outfam = NONE;
    if (ext_is(inext, "docx") || ext_is(inext, "md") || ext_is(inext, "rtf") ||
        ext_is(inext, "html") || ext_is(inext, "odt") || ext_is(inext, "fodt") ||
        ext_is(inext, "doc")) infam = TEXT;
    else if (ext_is(inext, "xlsx") || ext_is(inext, "csv") || ext_is(inext, "tsv") ||
             ext_is(inext, "ods") || ext_is(inext, "fods") || ext_is(inext, "xls")) infam = SHEET;
    else if (ext_is(inext, "pptx") || ext_is(inext, "odp") || ext_is(inext, "fodp") ||
             ext_is(inext, "ppt")) infam = SHOW;

    if (ext_is(outext, "docx") || ext_is(outext, "md") || ext_is(outext, "html") ||
        ext_is(outext, "rtf") || ext_is(outext, "odt") || ext_is(outext, "fodt") ||
        ext_is(outext, "pdf") || ext_is(outext, "epub") || ext_is(outext, "json")) {
        /* json is emitted in the input family's model (doc/book/pres) */
        if (ext_is(outext, "json")) outfam = infam;
        else outfam = TEXT;
    } else if (ext_is(outext, "xlsx") || ext_is(outext, "csv") || ext_is(outext, "tsv") ||
               ext_is(outext, "ods") || ext_is(outext, "fods")) outfam = SHEET;
    else if (ext_is(outext, "pptx") || ext_is(outext, "odp") || ext_is(outext, "fodp")) outfam = SHOW;
    else outfam = NONE;

    int rc = -1;

    if (infam == TEXT && outfam == TEXT) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == SHEET && outfam == SHEET) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_book_json(b, out_path);
            else rc = write_sheet(out_path, outext, b);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == SHOW) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
        }
        wubushow_free(p);
    }
    /* cross-family bridges */
    else if (infam == SHEET && outfam == TEXT) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            dm_doc d; sheet_to_text(b, &d);
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
            wubuedit_docmodel_free(&d);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == TEXT) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            dm_doc d; show_to_text(p, &d);
            if (ext_is(outext, "json")) rc = wubudoc_write_doc_json(&d, out_path);
            else rc = write_text(out_path, outext, &d);
            wubuedit_docmodel_free(&d);
        }
        wubushow_free(p);
    }
    else if (infam == TEXT && outfam == SHEET) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            wubucell_book *b = wubucell_create();
            text_to_sheet(&d, b);
            rc = write_sheet(out_path, outext, b);
            wubucell_free(b);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == TEXT && outfam == SHOW) {
        dm_doc d; memset(&d, 0, sizeof d);
        if (read_text(in_path, inext, &d) == 0) {
            wubushow_pres *p = wubushow_create();
            text_to_show(&d, p);
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
            wubushow_free(p);
        }
        wubuedit_docmodel_free(&d);
    }
    else if (infam == SHEET && outfam == SHOW) {
        wubucell_book *b = NULL;
        if (read_sheet(in_path, inext, &b) == 0) {
            dm_doc d; sheet_to_text(b, &d);
            wubushow_pres *p = wubushow_create();
            text_to_show(&d, p);
            if (ext_is(outext, "json")) rc = wubudoc_write_pres_json(p, out_path);
            else rc = write_show(out_path, outext, p);
            wubushow_free(p);
            wubuedit_docmodel_free(&d);
        }
        wubucell_free(b);
    }
    else if (infam == SHOW && outfam == SHEET) {
        wubushow_pres *p = NULL;
        if (read_show(in_path, inext, &p) == 0) {
            dm_doc d; show_to_text(p, &d);
            wubucell_book *b = wubucell_create();
            text_to_sheet(&d, b);
            if (ext_is(outext, "json")) rc = wubudoc_write_book_json(b, out_path);
            else rc = write_sheet(out_path, outext, b);
            wubucell_free(b);
            wubuedit_docmodel_free(&d);
        }
        wubushow_free(p);
    }
    else {
        fprintf(stderr, "wubuconv: unsupported conversion %s -> %s\n", inext, outext);
    }

    free(inext); free(outext);
    return rc;
}
