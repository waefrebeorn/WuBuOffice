/* odf_pres.c -- OpenDocument Presentation (.odp) writer + reader. See odf.h.
 * Clean-room C11. Model: wubushow_pres. Body XML from shared odf_body. */

#include "odf.h"
#include "odf_body.h"
#include "../../src/wubuxml/parser.h"
#include "../../src/wubuoxml/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wubuodf_write_odp(const wubushow_pres *pres, const char *path) {
    if (!pres) return -1;
    odf_sbuf b = {0};
    odf_sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content ");
    odf_sb_puts(&b, WUBUODF_NS_ALL);
    odf_sb_puts(&b, " office:version=\"1.3\">\n");
    wubuodf_emit_pres_body(&b, pres);
    odf_sb_puts(&b, "</office:document-content>\n");
    int rc = wubuodf_assemble(path, "application/vnd.oasis.opendocument.presentation", b.s, b.n);
    free(b.s);
    return rc;
}

/* ---- reader ---- */

typedef struct {
    wubushow_pres *pres;
    int in_page;
    int frame_is_title;    /* current frame is the title class */
    int frame_is_body;
    int in_p;
    char *title; size_t tin, tcap;
    char *body;  size_t bin, bcap;
    char *para;  size_t pin, pcap;
} odp_state;

static void acc(char **buf, size_t *n, size_t *cap, const char *s, size_t len) {
    if (*n + len + 1 > *cap) { while (*n + len + 1 > *cap) *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap); }
    memcpy(*buf + *n, s, len); *n += len; (*buf)[*n] = '\0';
}

static int odp_ev(wubuxml_event evt, const wubuxml_info *info, void *user) {
    odp_state *st = (odp_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "draw:page") == 0) {
            st->in_page = 1;
            st->tin = 0; if (st->title) st->title[0] = '\0';
            st->bin = 0; if (st->body) st->body[0] = '\0';
        } else if (strcmp(name, "draw:frame") == 0) {
            st->frame_is_title = 0; st->frame_is_body = 0;
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "presentation:class") == 0) {
                    if (strcmp(info->attr_val[a], "title") == 0) st->frame_is_title = 1;
                    else st->frame_is_body = 1;   /* outline/subtitle/notes -> body */
                }
        } else if (strcmp(name, "text:p") == 0) {
            st->in_p = 1; st->pin = 0; if (st->para) st->para[0] = '\0';
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->in_p) acc(&st->para, &st->pin, &st->pcap, info->text, info->text_len);
        return 0;
    }

    if (strcmp(name, "text:p") == 0) {
        if (st->in_p) {
            if (st->frame_is_title) {
                acc(&st->title, &st->tin, &st->tcap, st->para ? st->para : "", st->para ? strlen(st->para) : 0);
            } else if (st->frame_is_body) {
                if (st->bin > 0) acc(&st->body, &st->bin, &st->bcap, "\n", 1);
                acc(&st->body, &st->bin, &st->bcap, st->para ? st->para : "", st->para ? strlen(st->para) : 0);
            }
            st->in_p = 0;
        }
    } else if (strcmp(name, "draw:frame") == 0) {
        st->frame_is_title = 0; st->frame_is_body = 0;
    } else if (strcmp(name, "draw:page") == 0) {
        wubushow_slide(st->pres, st->title ? st->title : "", st->body ? st->body : "");
        st->in_page = 0;
    }
    return 0;
}

int wubuodf_read_odp(const char *path, wubushow_pres **out) {
    if (!out) return -1;
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
    wubushow_pres *pres = wubushow_create();
    if (content && pres) rc = wubuodf_parse_pres_xml(content->bytes, content->len, pres);
    wubuoxml_free(&pkg);
    free(data);
    if (rc == 0) *out = pres; else wubushow_free(pres);
    return rc;
}

/* Shared XML-bytes entry: run the ODP SAX handler over `bytes` into `pres`.
 * Used by the packaged reader (content.xml) and the flat .fodp reader. */
int wubuodf_parse_pres_xml(const uint8_t *bytes, size_t len, wubushow_pres *pres) {
    if (!pres || !bytes) return -1;
    odp_state st; memset(&st, 0, sizeof st);
    st.pres = pres;
    int rc = wubuxml_parse(bytes, len, odp_ev, &st);
    free(st.title); free(st.body); free(st.para);
    return rc;
}
