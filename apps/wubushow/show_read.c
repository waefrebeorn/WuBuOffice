/* show_read.c -- read a .pptx into a wubushow_pres.
 *
 * See show_read.h for the contract. Self-contained: it carries its own tiny
 * presentation.xml / .rels scanners (consistent with cell_read.c) so the pptx
 * reader does not depend on the package reader's optional relationship fields.
 *
 * Clean-room, from-scratch (SLERM). */

#include "show_read.h"
#include "show.h"
#include "show_internal.h"
#include "reader.h"
#include "../../src/wubuxml/parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* growable NUL-terminated string buffer */
static void sb_pushs(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 64;
        while (*len + n + 1 > nc) nc *= 2;
        char *nb = realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n); *len += n; (*buf)[*len] = '\0';
}
static void sb_pushc(char **buf, size_t *len, size_t *cap, char c) { sb_pushs(buf, len, cap, &c, 1); }

/* ---- presentation.xml / .rels bookkeeping (minimal, self-contained) ---- */

typedef struct { char rid[32]; char target[256]; } sh_rel;
typedef struct { char rid[32]; } sh_sldid;

static void rels_collect(const uint8_t *d, size_t n, sh_rel *out, size_t *cnt, size_t cap) {
    const char *s = (const char *)d;
    *cnt = 0;
    for (size_t i = 0; i + 12 <= n; ) {
        if (strncmp(s + i, "<Relationship", 12) != 0) { i++; continue; }
        char rid[32] = "", tgt[256] = "";
        const char *p = s + i, *q;
        if ((q = strstr(p, "Id=\"")))     { q += 4; size_t j = 0; while (*q && *q != '"' && j < 31) rid[j++] = *q++; rid[j] = '\0'; }
        if ((q = strstr(p, "Target=\""))) { q += 8; size_t j = 0; while (*q && *q != '"' && j < 255) tgt[j++] = *q++; tgt[j] = '\0'; }
        if (rid[0] && tgt[0] && *cnt < cap) {
            strncpy(out[*cnt].rid, rid, sizeof out[*cnt].rid - 1); out[*cnt].rid[sizeof out[*cnt].rid - 1] = '\0';
            strncpy(out[*cnt].target, tgt, sizeof out[*cnt].target - 1); out[*cnt].target[sizeof out[*cnt].target - 1] = '\0';
            (*cnt)++;
        }
        const char *end = strstr(p, "/>"); if (!end) end = strstr(p, ">");
        if (!end) break;
        i = (size_t)(end - s) + 1;
    }
}

static void sldid_collect(const uint8_t *d, size_t n, sh_sldid *out, size_t *cnt, size_t cap) {
    const char *s = (const char *)d;
    *cnt = 0;
    for (size_t i = 0; i + 5 <= n; ) {
        if (strncmp(s + i, "<p:sldId", 8) != 0) { i++; continue; }
        char rid[32] = "";
        const char *p = s + i, *q;
        if ((q = strstr(p, "r:id=\""))) { q += 6; size_t j = 0; while (*q && *q != '"' && j < 31) rid[j++] = *q++; rid[j] = '\0'; }
        else if ((q = strstr(p, "r:id='"))) { q += 6; size_t j = 0; while (*q && *q != '\'' && j < 31) rid[j++] = *q++; rid[j] = '\0'; }
        if (rid[0] && *cnt < cap) {
            strncpy(out[*cnt].rid, rid, sizeof out[*cnt].rid - 1); out[*cnt].rid[sizeof out[*cnt].rid - 1] = '\0';
            (*cnt)++;
        }
        const char *end = strstr(p, "/>"); if (!end) end = strstr(p, ">");
        if (!end) break;
        i = (size_t)(end - s) + 1;
    }
}

static const char *resolve_target(const sh_rel *rels, size_t nrel, const char *rid, char *buf, size_t buflen) {
    for (size_t i = 0; i < nrel; i++) {
        if (strcmp(rels[i].rid, rid) == 0) {
            const char *t = rels[i].target;
            if (t[0] == '/') snprintf(buf, buflen, "%s", t + 1);
            else if (strncmp(t, "ppt/", 4) == 0) snprintf(buf, buflen, "%s", t);
            else snprintf(buf, buflen, "ppt/%s", t);   /* relative to ppt/ */
            return buf;
        }
    }
    return NULL;
}

/* ---- per-slide SAX state ---- */

typedef enum { SP_UNKNOWN, SP_TITLE, SP_BODY } sp_kind;

typedef struct {
    sp_kind shape;          /* current shape kind */
    int     in_t;           /* inside <a:t>: only run text is real content */
    char *title; size_t tlen, tcap;
    char *body;  size_t blen, bcap;     /* full body text (bullets joined) */
    char *brun;  size_t brlen, brcap;   /* current bullet run (within a <a:p>) */
} slide_state;

static void reset_slide(slide_state *st) {
    st->shape = SP_UNKNOWN;
    st->in_t = 0;
    st->brlen = 0; if (st->brun) st->brun[0] = '\0';
}

static int slide_on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    slide_state *st = (slide_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (strcmp(name, "p:cNvPr") == 0) {
            /* Fallback signal: our own writer names the title shape "Title"
             * (with an empty <p:nvPr/>, no placeholder). Match a "Title"
             * prefix so producer variants like "Title 1" are caught too. */
            for (int a = 0; a < info->attr_count; a++) {
                if (strcmp(info->attr_name[a], "name") == 0) {
                    if (strncmp(info->attr_val[a], "Title", 5) == 0) st->shape = SP_TITLE;
                    else st->shape = SP_BODY;   /* any non-title text shape is body */
                }
            }
        } else if (strcmp(name, "p:ph") == 0) {
            /* Authoritative signal (foreign producers: python-pptx, PowerPoint,
             * LibreOffice): the placeholder type. type="title"/"ctrTitle" is the
             * slide title; anything else (body, subTitle, idx-only) is body. */
            const char *type = NULL;
            for (int a = 0; a < info->attr_count; a++)
                if (strcmp(info->attr_name[a], "type") == 0) type = info->attr_val[a];
            if (type && (strcmp(type, "title") == 0 || strcmp(type, "ctrTitle") == 0))
                st->shape = SP_TITLE;
            else
                st->shape = SP_BODY;
        } else if (strcmp(name, "a:t") == 0) {
            st->in_t = 1;
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        /* Only capture text that lives inside a real <a:t> run. Inter-element
         * whitespace/newlines are also delivered as TEXT events; we ignore them
         * so titles and bullets are not polluted with formatting newlines. */
        if (st->in_t) {
            if (st->shape == SP_TITLE) {
                sb_pushs(&st->title, &st->tlen, &st->tcap, info->text, info->text_len);
            } else if (st->shape == SP_BODY) {
                sb_pushs(&st->brun, &st->brlen, &st->brcap, info->text, info->text_len);
            }
        }
        return 0;
    }

    /* WUBUXML_EVT_END */
    if (strcmp(name, "a:t") == 0) {
        st->in_t = 0;
    } else if (strcmp(name, "a:p") == 0) {
        if (st->shape == SP_BODY && st->brlen > 0) {
            sb_pushs(&st->body, &st->blen, &st->bcap, st->brun, st->brlen);
            sb_pushc(&st->body, &st->blen, &st->bcap, '\n');
            st->brlen = 0; if (st->brun) st->brun[0] = '\0';
        }
    } else if (strcmp(name, "p:sp") == 0) {
        reset_slide(st);
    }
    return 0;
}

int wubushow_read(const char *path, wubushow_pres **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { free(data); return -1; }

    wubushow_pres *p = wubushow_create();
    if (!p) { wubuoxml_free(&pkg); free(data); return -1; }

    /* locate slides via presentation.xml + its rels (the spec path) */
    sh_sldid ids[512]; size_t nids = 0;
    sh_rel   rels[512]; size_t nrels = 0;
    char tgt[320];

    const wubuoxml_part *pres = wubuoxml_part_find(&pkg, "ppt/presentation.xml");
    if (pres) {
        sldid_collect(pres->bytes, pres->len, ids, &nids, 512);
        const wubuoxml_part *pr = wubuoxml_part_find(&pkg, "ppt/_rels/presentation.xml.rels");
        if (pr) rels_collect(pr->bytes, pr->len, rels, &nrels, 512);
    }

    for (size_t i = 0; i < nids; i++) {
        const char *partname = resolve_target(rels, nrels, ids[i].rid, tgt, sizeof tgt);
        const wubuoxml_part *pt = partname ? wubuoxml_part_find(&pkg, partname) : NULL;
        if (!pt) continue;

        slide_state ss; memset(&ss, 0, sizeof ss);
        if (wubuxml_parse(pt->bytes, pt->len, slide_on_event, &ss) != 0) {
            free(ss.title); free(ss.body); free(ss.brun);
            continue;   /* best-effort: skip a malformed slide */
        }
        /* strip a single trailing newline from body (writer re-adds on emit) */
        if (ss.blen > 0 && ss.body[ss.blen - 1] == '\n') ss.body[--ss.blen] = '\0';
        wubushow_slide(p, ss.title ? ss.title : "", ss.body ? ss.body : "");
        free(ss.title); free(ss.body); free(ss.brun);
    }

    /* fallback: no presentation.xml parsed -- scan slides in numeric order */
    if (nids == 0) {
        for (size_t i = 1; i < 1000; i++) {
            snprintf(tgt, sizeof tgt, "ppt/slides/slide%zu.xml", i);
            const wubuoxml_part *pt = wubuoxml_part_find(&pkg, tgt);
            if (!pt) break;
            slide_state ss; memset(&ss, 0, sizeof ss);
            wubuxml_parse(pt->bytes, pt->len, slide_on_event, &ss);
            if (ss.blen > 0 && ss.body[ss.blen - 1] == '\n') ss.body[--ss.blen] = '\0';
            wubushow_slide(p, ss.title ? ss.title : "", ss.body ? ss.body : "");
            free(ss.title); free(ss.body); free(ss.brun);
        }
    }

    wubuoxml_free(&pkg);
    free(data);
    *out = p;
    return 0;
}
