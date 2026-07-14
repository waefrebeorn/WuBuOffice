/* read_epub.c -- EPUB reader -> dm_doc for WuBuOffice convert.
 *
 * EPUB is a ZIP container holding XHTML content documents. We open the archive
 * with wubuzip, locate the OPF package (OEBPS/content.opf or content.opf) to
 * learn the spine order, then feed each XHTML document in order through the
 * HTML reader (wuburead_html) which builds dm_doc. If no OPF is present we
 * fall back to all *.xhtml / *.html parts in the archive (alphabetical).
 *
 * Self-contained: reuses wubuzip + wuburead_html; builds dm_doc only through
 * wubuedit_docmodel_*. Clean-room C11. */

#include "readers.h"
#include "../wubuedit/docmodel.h"
#include "../../src/wubuzip/reader.h"
#include "../../src/wubuxml/parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal SAX to pull the spine order (itemref idref=...) plus the manifest
 * (item id=... href=...), then resolve each spine idref to its real href. */
typedef struct {
    char hrefs[64][256];   /* resolved content-doc hrefs, in spine order */
    int  n;
    /* manifest: id -> href */
    char m_id[128][128];
    char m_href[128][256];
    int  m_n;
    char cur_id[128];      /* id of the <item> currently being opened */
} opf_state;

static int on_opf(wubuxml_event evt, const wubuxml_info *info, void *user) {
    opf_state *st = (opf_state *)user;
    if (evt == WUBUXML_EVT_START) {
        if (strcmp(info->name, "item") == 0) {
            st->cur_id[0] = '\0';
            char href[256] = "";
            for (int a = 0; a < info->attr_count; a++) {
                if (strcmp(info->attr_name[a], "id") == 0)
                    snprintf(st->cur_id, sizeof st->cur_id, "%s", info->attr_val[a]);
                else if (strcmp(info->attr_name[a], "href") == 0)
                    snprintf(href, sizeof href, "%s", info->attr_val[a]);
            }
            if (st->cur_id[0] && st->m_n < 128) {
                snprintf(st->m_id[st->m_n], sizeof st->m_id[0], "%s", st->cur_id);
                snprintf(st->m_href[st->m_n], sizeof st->m_href[0], "%s", href);
                st->m_n++;
            }
        } else if (strcmp(info->name, "itemref") == 0) {
            const char *idref = NULL;
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "idref") == 0) idref = info->attr_val[a];
            if (idref && st->n < 64) {
                /* resolve idref -> href via the manifest */
                char href[256]; href[0] = '\0';
                for (int m = 0; m < st->m_n; m++)
                    if (strcmp(st->m_id[m], idref) == 0) { snprintf(href, sizeof href, "%s", st->m_href[m]); break; }
                snprintf(st->hrefs[st->n], sizeof st->hrefs[0], "%s", href[0] ? href : idref);
                st->n++;
            }
        }
    }
    return 0;
}

int wuburead_epub(const char *path, dm_doc *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuzip_archive z;
    if (wubuzip_open(data, (size_t)sz, &z) != 0) { free(data); return -1; }

    /* find OPF */
    size_t opf_i = (size_t)-1;
    for (size_t i = 0; i < wubuzip_count(&z); i++)
        if (strstr(wubuzip_name(&z, i), "content.opf")) { opf_i = i; break; }

    /* collect XHTML content documents in spine order (or alphabetical) */
    char parts[64][384]; int np = 0;
    if (opf_i != (size_t)-1) {
        uint8_t *opf; size_t olen;
        if (wubuzip_extract(&z, opf_i, &opf, &olen) == 0) {
            opf_state st; memset(&st, 0, sizeof st);
            wubuxml_parse(opf, olen, on_opf, &st);
            for (int k = 0; k < st.n && np < 64; k++) {
                /* href is relative to OEBPS/; keep just the filename tail so we
                 * can find the part anywhere in the archive. */
                const char *slash = strrchr(st.hrefs[k], '/');
                snprintf(parts[np], sizeof parts[0], "%s", slash ? slash + 1 : st.hrefs[k]);
                np++;
            }
            free(opf);
        }
    }
    if (np == 0) {
        for (size_t i = 0; i < wubuzip_count(&z) && np < 64; i++) {
            const char *nm = wubuzip_name(&z, i);
            if (strstr(nm, ".xhtml") || strstr(nm, ".html")) {
                snprintf(parts[np], sizeof parts[0], "%s", nm); np++;
            }
        }
    }

    int rc = 0;
    for (int k = 0; k < np; k++) {
        /* locate the part (try as-given, then under OEBPS/, then anywhere) */
        size_t idx = wubuzip_find(&z, parts[k]);
        if (idx == (size_t)-1) {
            for (size_t i = 0; i < wubuzip_count(&z); i++)
                if (strstr(wubuzip_name(&z, i), parts[k])) { idx = i; break; }
        }
        if (idx == (size_t)-1) continue;
        uint8_t *xb; size_t xl;
        if (wubuzip_extract(&z, idx, &xb, &xl) != 0) continue;
        /* feed through HTML reader by writing to a temp buffer parse.
         * wuburead_html takes a path, so parse in-memory: reuse its SAX by a
         * tiny inline call would duplicate; instead write temp file. */
        /* Use a heap-backed HTML parse to avoid temp files: call wuburead_html
         * on a temp path. Simpler: extract to a temp file. */
        char tmp[256]; snprintf(tmp, sizeof tmp, "/tmp/wubu_epub_%d.xhtml", k);
        FILE *tf = fopen(tmp, "wb");
        if (tf) { fwrite(xb, 1, xl, tf); fclose(tf); }
        dm_doc sub; memset(&sub, 0, sizeof sub);
        if (wuburead_html(tmp, &sub) == 0) {
            /* splice sub blocks into out (deep-copy: sub is freed below) */
            for (size_t b = 0; b < sub.n; b++) {
                dm_block *src = &sub.blocks[b];
                if (src->kind == DM_BLOCK_PARA) {
                    wubuedit_docmodel_add_para(out, src->para.style, src->para.bold, src->para.text);
                } else if (src->kind == DM_BLOCK_TABLE) {
                    size_t ncells = src->table.rows * src->table.cols;
                    dm_para **cp = NULL;
                    if (ncells) {
                        cp = calloc(ncells, sizeof *cp);
                        for (size_t i = 0; i < ncells; i++) {
                            dm_para *o = src->table.cells[i];
                            dm_para *d = calloc(1, sizeof *d);
                            d->text = o && o->text ? strdup(o->text) : strdup("");
                            d->bold = o ? o->bold : 0;
                            cp[i] = d;
                        }
                    }
                    wubuedit_docmodel_add_table(out, cp, src->table.rows, src->table.cols);
                }
            }
        }
        wubuedit_docmodel_free(&sub);
        remove(tmp);
        free(xb);
    }

    wubuzip_close(&z);
    free(data);
    return rc;
}
