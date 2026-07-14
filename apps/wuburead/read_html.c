/* read_html.c -- HTML reader -> dm_doc for WuBuOffice convert.
 *
 * HTML is tag-based; we reuse the project's SAX (wubuxml_parse) which already
 * delivers START/TEXT/END events with namespace-free names, and walk the
 * familiar block structure: <p> -> paragraph, <h1>..<h3> -> Heading styles,
 * <strong>/<b> -> bold, <table>/<tr>/<td>/<th> -> DM_BLOCK_TABLE. <br> ends
 * the current line.
 *
 * Table rows are assembled into a rectangular (row-major) cell grid; rows with
 * fewer cells than the widest row are NULL-padded, matching dm_doc's layout.
 *
 * Self-contained: builds dm_doc only through wubuedit_docmodel_* builders.
 * Clean-room C11. */

#include "readers.h"
#include "../wubuedit/docmodel.h"
#include "../../src/wubuxml/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* forward decls */
static uint8_t *html_normalize(const uint8_t *in, size_t len);

typedef enum { H_TOP, H_TABLE, H_ROW, H_CELL } h_ctx;

typedef struct {
    dm_doc  *doc;
    h_ctx    ctx;
    /* current paragraph */
    char    *para; size_t para_n, para_cap;
    int      bold;
    int      in_text;        /* inside an element that yields text */
    /* table assembly */
    dm_para **rowcells; size_t rown, rowcap;     /* cells of the row being built */
    size_t   tabcols;                            /* max cols across all rows */
    dm_para **flat; size_t flatcap, flatn;       /* finished rectangular grid */
} h_state;

static void pa_reset(h_state *s) { s->para_n = 0; if (s->para) s->para[0] = '\0'; s->bold = 0; s->in_text = 0; }
static void pa_append(h_state *s, const char *src, size_t n) {
    if (!n) return;
    if (s->para_n + n + 1 > s->para_cap) {
        size_t nc = s->para_cap ? s->para_cap * 2 : 64;
        while (s->para_n + n + 1 > nc) nc *= 2;
        s->para = realloc(s->para, nc); s->para_cap = nc;
    }
    memcpy(s->para + s->para_n, src, n); s->para_n += n; s->para[s->para_n] = '\0';
}
static void pa_flush(h_state *s, const char *style) {
    if (s->para_n == 0 && !style) return;
    wubuedit_docmodel_add_para(s->doc, style, s->bold, s->para ? s->para : "");
    pa_reset(s);
}

/* finish a table row: append its cells (length rown) to the rectangular grid,
 * growing tabcols to the widest row seen. */
static void row_commit(h_state *s) {
    if (s->tabcols < s->rown) s->tabcols = s->rown;
    if (s->flatn + s->rown > s->flatcap) {
        size_t nc = s->flatcap ? s->flatcap * 2 : 8;
        while (s->flatn + s->rown > nc) nc *= 2;
        s->flat = realloc(s->flat, nc * sizeof(dm_para *)); s->flatcap = nc;
    }
    for (size_t i = 0; i < s->rown; i++) s->flat[s->flatn++] = s->rowcells[i];
    /* ownership of the cell pointers transfers to s->flat; free the row
     * container itself and reset for the next row. */
    free(s->rowcells); s->rowcells = NULL;
    s->rown = 0;
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    h_state *s = (h_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "p") == 0) { s->in_text = 1; }
        else if (strcmp(name, "h1") == 0 || strcmp(name, "h2") == 0 || strcmp(name, "h3") == 0) { s->in_text = 1; }
        else if (strcmp(name, "strong") == 0 || strcmp(name, "b") == 0) { s->bold = 1; s->in_text = 1; }
        else if (strcmp(name, "br") == 0) { pa_append(s, "\n", 1); }
        else if (strcmp(name, "table") == 0) {
            s->ctx = H_TABLE; s->tabcols = 0; s->flatn = 0; s->flatcap = 0; s->flat = NULL;
        }
        else if (strcmp(name, "tr") == 0) { if (s->ctx == H_TABLE) { s->ctx = H_ROW; s->rown = 0; s->rowcap = 0; s->rowcells = NULL; } }
        else if (strcmp(name, "td") == 0 || strcmp(name, "th") == 0) {
            if (s->ctx == H_ROW) { s->ctx = H_CELL; s->bold = (name[1] == 'h'); s->para_n = 0; if (s->para) s->para[0]='\0'; }
        }
        return 0;
    }
    if (evt == WUBUXML_EVT_TEXT) {
        if (s->in_text || s->ctx == H_CELL) pa_append(s, info->text, info->text_len);
        return 0;
    }
    /* END */
    if (strcmp(name, "p") == 0) { if (s->ctx == H_TOP) pa_flush(s, NULL); s->in_text = 0; }
    else if (strcmp(name, "h1") == 0) { if (s->ctx == H_TOP) pa_flush(s, "Heading1"); s->in_text = 0; }
    else if (strcmp(name, "h2") == 0) { if (s->ctx == H_TOP) pa_flush(s, "Heading2"); s->in_text = 0; }
    else if (strcmp(name, "h3") == 0) { if (s->ctx == H_TOP) pa_flush(s, "Heading3"); s->in_text = 0; }
    else if (strcmp(name, "strong") == 0 || strcmp(name, "b") == 0) { s->bold = 0; }
    else if (strcmp(name, "td") == 0 || strcmp(name, "th") == 0) {
        if (s->ctx == H_CELL) {
            dm_para *cp = calloc(1, sizeof *cp);
            cp->text = s->para ? strdup(s->para) : strdup("");
            cp->bold = s->bold;
            if (s->rown == s->rowcap) { size_t nc = s->rowcap ? s->rowcap*2 : 4; s->rowcells = realloc(s->rowcells, nc*sizeof*s->rowcells); s->rowcap = nc; }
            s->rowcells[s->rown++] = cp;
            s->ctx = H_ROW;
        }
    }
    else if (strcmp(name, "tr") == 0) { if (s->ctx == H_ROW) { row_commit(s); s->ctx = H_TABLE; } }
    else if (strcmp(name, "table") == 0) {
        if (s->ctx == H_TABLE) {
            if (s->flatn > 0)
                wubuedit_docmodel_add_table(s->doc, s->flat, s->flatn / s->tabcols, s->tabcols);
            s->flat = NULL;
            s->ctx = H_TOP;
        }
    }
    return 0;
}

int wuburead_html(const char *path, dm_doc *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *raw = malloc(sz ? (size_t)sz + 1 : 1);
    if (!raw) { fclose(f); return -1; }
    if (fread(raw, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(raw); return -1; }
    fclose(f);
    raw[sz] = '\0';

    /* The project SAX is an XML parser; real-world HTML uses bare void
     * elements (<meta>, <br>, <img>, ...) and a DOCTYPE/comments. Normalize to
     * XML-parseable form: drop DOCTYPE + comments, and self-close void
     * elements. Self-contained, no second copy of any parser. */
    uint8_t *data = html_normalize(raw, (size_t)sz);
    free(raw);
    if (!data) return -1;
    size_t dlen = strlen((const char *)data);

    h_state s; memset(&s, 0, sizeof s); s.doc = out;
    int rc = wubuxml_parse(data, dlen, on_event, &s);
    if (s.para_n) pa_flush(&s, NULL);
    free(s.para); free(s.rowcells); free(s.flat);
    free(data);
    return rc == 0 ? 0 : -1;
}

/* ---- HTML -> XML normalization ----------------------------------------
 * Returns a heap buffer (caller frees) or NULL on OOM. */
static int is_void(const char *name, size_t n) {
    static const char *voids[] = {"meta","br","img","hr","link","input","area","base","col","embed","source","track","wbr"};
    for (size_t i = 0; i < sizeof voids / sizeof voids[0]; i++) {
        size_t vn = strlen(voids[i]);
        if (n == vn && strncmp(name, voids[i], vn) == 0) return 1;
    }
    return 0;
}

static uint8_t *html_normalize(const uint8_t *in, size_t len) {
    /* worst case: every char becomes '&amp;' (5x) -- generous upper bound. */
    size_t cap = len * 5 + 16;
    uint8_t *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    size_t i = 0;
    /* skip a leading <!DOCTYPE ... > (case-insensitive) */
    while (i < len) {
        if (i + 9 < len && (in[i]=='<' ) && (in[i+1]=='!'||in[i+1]=='?')) {
            /* find matching > */
            size_t j = i + 1;
            while (j < len && in[j] != '>') j++;
            i = (j < len) ? j + 1 : len;
            continue;
        }
        /* comment <!-- ... --> */
        if (i + 4 < len && in[i]=='<' && in[i+1]=='!' && in[i+2]=='-' && in[i+3]=='-') {
            size_t j = i + 4;
            while (j + 2 < len && !(in[j]=='-' && in[j+1]=='-' && in[j+2]=='>')) j++;
            i = (j + 2 < len) ? j + 3 : len;
            continue;
        }
        if (in[i] == '<') {
            /* copy the tag, but detect void elements with no '/>' closer */
            size_t j = i + 1;
            while (j < len && in[j] != '>' && in[j] != '<') j++;
            if (j < len && in[j] == '>') {
                /* tag content is in[i+1 .. j-1] (excluding the '<' and '>') */
                const uint8_t *tc = in + i + 1;
                size_t tcl = (j - 1) - (i + 1) + 1; /* up to the '>' */
                /* tag name = leading run of [a-zA-Z0-9] */
                size_t nm = 0;
                while (nm < tcl && ((tc[nm]>='a'&&tc[nm]<='z')||(tc[nm]>='A'&&tc[nm]<='Z')||(tc[nm]>='0'&&tc[nm]<='9'))) nm++;
                int selfclosed = (tcl > 0 && tc[tcl-1] == '/');
                int closing = (tcl > 0 && tc[0] == '/');
                /* emit the opening '<' */
                out[o++] = '<';
                /* copy tag content verbatim */
                for (size_t k = 0; k < tcl; k++) out[o++] = tc[k];
                /* void element, not already self-closed, not a closing tag:
                 * rewrite to self-closing by inserting '/' before '>' */
                if (!closing && !selfclosed && is_void((const char *)tc, nm)) {
                    if (o + 1 >= cap) { uint8_t *n = realloc(out, cap *= 2); if (!n) { free(out); return NULL; } out = n; }
                    out[o++] = '/';
                }
                out[o++] = '>';
                i = j + 1;
                continue;
            }
            /* malformed '<' with no '>': emit as text */
        }
        /* ordinary byte: copy verbatim (text) */
        if (o + 1 >= cap) { uint8_t *n = realloc(out, cap *= 2); if (!n) { free(out); return NULL; } out = n; }
        out[o++] = in[i++];
    }
    out[o] = '\0';
    return out;
}
