/* odf_pres.c -- OpenDocument Presentation (.odp) writer + reader. See odf.h.
 * Clean-room C11. Model: wubushow_pres. */

#include "odf.h"
#include "../../src/wubuxml/parser.h"
#include "../../src/wubuoxml/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *s; size_t n, cap; } sbuf;
static void sb_putn(sbuf *b, const char *p, size_t n) {
    if (b->n + n + 1 > b->cap) { while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 1024; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, p, n); b->n += n; b->s[b->n] = '\0';
}
static void sb_puts(sbuf *b, const char *s) { sb_putn(b, s, strlen(s)); }
static void sb_esc(sbuf *b, const char *s) {
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
            case '&': sb_puts(b, "&amp;"); break;
            case '<': sb_puts(b, "&lt;"); break;
            case '>': sb_puts(b, "&gt;"); break;
            default: sb_putn(b, p, 1);
        }
    }
}

/* Emit each line of `body` as its own text:p inside the frame. */
static void emit_body(sbuf *b, const char *body) {
    const char *p = body ? body : "";
    const char *start = p;
    for (;; p++) {
        if (*p == '\n' || *p == '\0') {
            sb_puts(b, "<text:p>");
            /* escape [start,p) */
            for (const char *q = start; q < p; q++) {
                switch (*q) {
                    case '&': sb_puts(b, "&amp;"); break;
                    case '<': sb_puts(b, "&lt;"); break;
                    case '>': sb_puts(b, "&gt;"); break;
                    default: sb_putn(b, q, 1);
                }
            }
            sb_puts(b, "</text:p>");
            if (*p == '\0') break;
            start = p + 1;
        }
    }
}

int wubuodf_write_odp(const wubushow_pres *pres, const char *path) {
    if (!pres) return -1;
    sbuf b = {0};
    sb_puts(&b,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-content "
        "xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
        "xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\" "
        "xmlns:presentation=\"urn:oasis:names:tc:opendocument:xmlns:presentation:1.0\" "
        "xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
        "xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" "
        "office:version=\"1.3\">\n"
        "<office:body><office:presentation>\n");

    int ns = wubushow_slide_count(pres);
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(pres, i, &title, &body);
        sb_puts(&b, "<draw:page draw:name=\"");
        char nm[32]; snprintf(nm, sizeof nm, "Slide%d", i + 1); sb_puts(&b, nm);
        sb_puts(&b, "\">\n");
        /* title frame */
        sb_puts(&b, "<draw:frame presentation:class=\"title\" "
                    "svg:width=\"20cm\" svg:height=\"3cm\" svg:x=\"2cm\" svg:y=\"1cm\">"
                    "<draw:text-box><text:p>");
        sb_esc(&b, title ? title : "");
        sb_puts(&b, "</text:p></draw:text-box></draw:frame>\n");
        /* body frame */
        sb_puts(&b, "<draw:frame presentation:class=\"outline\" "
                    "svg:width=\"20cm\" svg:height=\"12cm\" svg:x=\"2cm\" svg:y=\"5cm\">"
                    "<draw:text-box>");
        emit_body(&b, body);
        sb_puts(&b, "</draw:text-box></draw:frame>\n");
        sb_puts(&b, "</draw:page>\n");
    }

    sb_puts(&b, "</office:presentation></office:body>\n</office:document-content>\n");
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
    if (content && pres) {
        odp_state st; memset(&st, 0, sizeof st);
        st.pres = pres;
        rc = wubuxml_parse(content->bytes, content->len, odp_ev, &st);
        free(st.title); free(st.body); free(st.para);
    }
    wubuoxml_free(&pkg);
    free(data);
    if (rc == 0) *out = pres; else wubushow_free(pres);
    return rc;
}
