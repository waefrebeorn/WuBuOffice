/* odf_flat.c -- Flat OpenDocument (.fodt / .fods / .fodp) writer + reader.
 *
 * Flat ODF is a single, self-contained XML file: the entire document is one
 * <office:document office:mimetype="..."> element with the same <office:body>
 * inner XML as the packaged formats. No ZIP, no manifest -- ideal for diffing,
 * version control, and piping.
 *
 * REUSE: writers emit the body via the shared odf_body emitters; readers run
 * the very same SAX handlers as the packaged readers (wubuodf_parse_*_xml),
 * directly over the whole file. Zero duplicated parse/emit logic.
 *
 * Clean-room C11. */

#include "odf.h"
#include "odf_body.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- writers ---- */

static int flat_write(const char *path, const char *mimetype,
                      void (*emit)(odf_sbuf *, const void *), const void *model) {
    if (!model) return -1;
    odf_sbuf b = {0};
    odf_sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document ");
    odf_sb_puts(&b, WUBUODF_NS_ALL);
    odf_sb_puts(&b, " office:version=\"1.3\" office:mimetype=\"");
    odf_sb_puts(&b, mimetype);
    odf_sb_puts(&b, "\">\n");
    emit(&b, model);
    odf_sb_puts(&b, "</office:document>\n");

    FILE *f = fopen(path, "wb");
    if (!f) { free(b.s); return -1; }
    size_t wrote = fwrite(b.s, 1, b.n, f);
    int rc = (wrote == b.n && fclose(f) == 0) ? 0 : -1;
    free(b.s);
    return rc;
}

/* thin adapters so the three model-typed emitters share flat_write */
static void emit_text(odf_sbuf *b, const void *m)  { wubuodf_emit_text_body(b, (const dm_doc *)m); }
static void emit_sheet(odf_sbuf *b, const void *m) { wubuodf_emit_sheet_body(b, (const wubucell_book *)m); }
static void emit_pres(odf_sbuf *b, const void *m)  { wubuodf_emit_pres_body(b, (const wubushow_pres *)m); }

int wubuodf_write_fodt(const dm_doc *d, const char *path) {
    return flat_write(path, "application/vnd.oasis.opendocument.text", emit_text, d);
}
int wubuodf_write_fods(const wubucell_book *b, const char *path) {
    return flat_write(path, "application/vnd.oasis.opendocument.spreadsheet", emit_sheet, b);
}
int wubuodf_write_fodp(const wubushow_pres *p, const char *path) {
    return flat_write(path, "application/vnd.oasis.opendocument.presentation", emit_pres, p);
}

/* ---- readers ---- */

/* slurp whole file into a malloc'd buffer; *len set; caller frees. */
static uint8_t *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *data = malloc((size_t)sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return NULL; }
    fclose(f);
    *len = (size_t)sz;
    return data;
}

int wubuodf_read_fodt(const char *path, dm_doc *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    size_t len = 0; uint8_t *data = slurp(path, &len);
    if (!data) return -1;
    int rc = wubuodf_parse_text_xml(data, len, out);
    free(data);
    return rc;
}

int wubuodf_read_fods(const char *path, wubucell_book **out) {
    if (!out) return -1;
    size_t len = 0; uint8_t *data = slurp(path, &len);
    if (!data) return -1;
    wubucell_book *bk = wubucell_create();
    int rc = bk ? wubuodf_parse_sheet_xml(data, len, bk) : -1;
    free(data);
    if (rc == 0) *out = bk; else wubucell_free(bk);
    return rc;
}

int wubuodf_read_fodp(const char *path, wubushow_pres **out) {
    if (!out) return -1;
    size_t len = 0; uint8_t *data = slurp(path, &len);
    if (!data) return -1;
    wubushow_pres *pres = wubushow_create();
    int rc = pres ? wubuodf_parse_pres_xml(data, len, pres) : -1;
    free(data);
    if (rc == 0) *out = pres; else wubushow_free(pres);
    return rc;
}
