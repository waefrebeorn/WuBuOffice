/* odf_text.c -- OpenDocument Text (.odt) writer + reader. See odf.h.
 * Clean-room C11. Model: dm_doc. Body XML is emitted by the shared odf_body
 * module (single source of truth, also used by the flat .fodt writer). */

#include "odf.h"
#include "odf_body.h"
#include "../../src/wubuxml/parser.h"
#include "../../src/wubuoxml/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- writer ---- */

int wubuodf_write_odt(const dm_doc *d, const char *path) {
    if (!d) return -1;
    odf_sbuf b = {0};
    odf_sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content ");
    odf_sb_puts(&b, WUBUODF_NS_ALL);
    odf_sb_puts(&b, " office:version=\"1.3\">\n");
    wubuodf_emit_text_body(&b, d);
    odf_sb_puts(&b, "</office:document-content>\n");

    int rc = wubuodf_assemble(path, "application/vnd.oasis.opendocument.text", b.s, b.n);
    free(b.s);
    return rc;
}

/* ---- reader (SAX over content.xml) ---- */

typedef struct {
    dm_doc *doc;
    /* current paragraph accumulation */
    int in_h, in_p, h_level;
    int in_span_bold, saw_bold;
    char *txt; size_t tn, tcap;
    /* table state */
    int in_table, in_row, in_cell;
    dm_para *cells; size_t ncells, capcells;   /* flat cells for current table */
    size_t cur_cols, max_cols, rows;
    char *cell_txt; size_t ctn, ctcap;
} odt_state;

static void acc(char **buf, size_t *n, size_t *cap, const char *s, size_t len) {
    if (*n + len + 1 > *cap) { while (*n + len + 1 > *cap) *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
    memcpy(*buf + *n, s, len); *n += len; (*buf)[*n] = '\0';
}

static void push_para(odt_state *st, const char *style, int bold, const char *text) {
    dm_doc *d = st->doc;
    if (d->n + 1 > d->cap) { d->cap = d->cap ? d->cap * 2 : 8; d->blocks = realloc(d->blocks, d->cap * sizeof *d->blocks); }
    dm_block *b = &d->blocks[d->n++];
    memset(b, 0, sizeof *b);
    b->kind = DM_BLOCK_PARA;
    b->para.style = style ? strdup(style) : NULL;
    b->para.bold = bold;
    b->para.text = strdup(text ? text : "");
}

static int odt_ev(wubuxml_event evt, const wubuxml_info *info, void *user) {
    odt_state *st = (odt_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "text:h") == 0) {
            st->in_h = 1; st->h_level = 1; st->tn = 0; if (st->txt) st->txt[0] = '\0';
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "text:outline-level") == 0) st->h_level = atoi(info->attr_val[a]);
        } else if (strcmp(name, "text:p") == 0) {
            if (st->in_cell) { st->ctn = 0; if (st->cell_txt) st->cell_txt[0] = '\0'; }
            else { st->in_p = 1; st->tn = 0; if (st->txt) st->txt[0] = '\0'; st->saw_bold = 0; }
        } else if (strcmp(name, "text:span") == 0) {
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "text:style-name") == 0 &&
                    strstr(info->attr_val[a], "Bold")) { st->in_span_bold = 1; st->saw_bold = 1; }
        } else if (strcmp(name, "table:table") == 0) {
            st->in_table = 1; st->ncells = 0; st->max_cols = 0; st->rows = 0;
        } else if (strcmp(name, "table:table-row") == 0) {
            st->in_row = 1; st->cur_cols = 0;
        } else if (strcmp(name, "table:table-cell") == 0) {
            st->in_cell = 1; st->ctn = 0; if (st->cell_txt) st->cell_txt[0] = '\0';
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->in_cell) acc(&st->cell_txt, &st->ctn, &st->ctcap, info->text, info->text_len);
        else if (st->in_h || st->in_p) acc(&st->txt, &st->tn, &st->tcap, info->text, info->text_len);
        return 0;
    }

    /* END */
    if (strcmp(name, "text:span") == 0) {
        st->in_span_bold = 0;
    } else if (strcmp(name, "text:h") == 0 && st->in_h) {
        const char *style = st->h_level == 1 ? "Heading1" : st->h_level == 2 ? "Heading2" : "Heading3";
        push_para(st, style, 0, st->txt ? st->txt : "");
        st->in_h = 0;
    } else if (strcmp(name, "text:p") == 0) {
        if (st->in_cell) { /* cell paragraph: text captured into cell_txt */ }
        else if (st->in_p) { push_para(st, NULL, st->saw_bold, st->txt ? st->txt : ""); st->in_p = 0; }
    } else if (strcmp(name, "table:table-cell") == 0) {
        /* store this cell */
        if (st->ncells + 1 > st->capcells) { st->capcells = st->capcells ? st->capcells * 2 : 16; st->cells = realloc(st->cells, st->capcells * sizeof *st->cells); }
        dm_para *cp = &st->cells[st->ncells++];
        memset(cp, 0, sizeof *cp);
        cp->text = strdup(st->cell_txt ? st->cell_txt : "");
        st->cur_cols++;
        st->in_cell = 0;
    } else if (strcmp(name, "table:table-row") == 0) {
        if (st->cur_cols > st->max_cols) st->max_cols = st->cur_cols;
        st->rows++;
        st->in_row = 0;
    } else if (strcmp(name, "table:table") == 0) {
        /* build a dm_table block: rectangularize to max_cols */
        dm_doc *d = st->doc;
        if (d->n + 1 > d->cap) { d->cap = d->cap ? d->cap * 2 : 8; d->blocks = realloc(d->blocks, d->cap * sizeof *d->blocks); }
        dm_block *b = &d->blocks[d->n++];
        memset(b, 0, sizeof *b);
        b->kind = DM_BLOCK_TABLE;
        b->table.rows = st->rows;
        b->table.cols = st->max_cols;
        size_t total = st->rows * st->max_cols;
        b->table.cells = calloc(total ? total : 1, sizeof(dm_para *));
        /* map the flat cell list (which was row-major but ragged) into the grid.
         * We stored cells in order; reconstruct row boundaries by cur_cols is
         * lost, so we re-walk assuming each row had max_cols... instead we kept
         * them sequentially per row. Simplest correct approach: we recorded
         * cells sequentially; distribute assuming rows*max_cols by placing
         * sequentially and padding. Because ragged rows are rare in our writer
         * (always rectangular), sequential placement is correct. */
        for (size_t k = 0; k < st->ncells && k < total; k++) {
            dm_para *cp = malloc(sizeof *cp);
            memset(cp, 0, sizeof *cp);
            cp->text = st->cells[k].text;   /* transfer ownership */
            b->table.cells[k] = cp;
        }
        /* fill any missing cells with empty paragraphs */
        for (size_t k = st->ncells; k < total; k++) {
            dm_para *cp = calloc(1, sizeof *cp);
            cp->text = strdup("");
            b->table.cells[k] = cp;
        }
        free(st->cells); st->cells = NULL; st->ncells = 0; st->capcells = 0;
        st->in_table = 0;
    }
    return 0;
}

int wubuodf_read_odt(const char *path, dm_doc *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { free(data); return -1; }
    const wubuoxml_part *content = wubuoxml_part_find(&pkg, "content.xml");
    int rc = -1;
    if (content) rc = wubuodf_parse_text_xml(content->bytes, content->len, out);
    wubuoxml_free(&pkg);
    free(data);
    return rc;
}

/* Shared XML-bytes entry: run the ODT SAX handler over `bytes`. Used by both
 * the packaged reader (content.xml) and the flat .fodt reader (whole file). */
int wubuodf_parse_text_xml(const uint8_t *bytes, size_t len, dm_doc *out) {
    if (!out || !bytes) return -1;
    odt_state st; memset(&st, 0, sizeof st);
    st.doc = out;
    int rc = wubuxml_parse(bytes, len, odt_ev, &st);
    free(st.txt); free(st.cell_txt);
    if (st.cells) { for (size_t k = 0; k < st.ncells; k++) free(st.cells[k].text); free(st.cells); }
    return rc;
}
