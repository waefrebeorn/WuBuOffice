/* read_rtf.c -- RTF reader -> dm_doc for WuBuOffice convert.
 *
 * RTF is a token stream (not XML): groups delimited by '{' '}', control words
 * like \par / \b / \pard, and plain text between them. We walk the byte stream
 * tracking current bold state and accumulating paragraph text, emitting a
 * DM_BLOCK_PARA per \par (and one for trailing text). Tables in RTF use the
 * \trowd / \cell / \row primitives; we collect cells within a row group and
 * emit a DM_BLOCK_TABLE.
 *
 * Self-contained: builds dm_doc only through wubuedit_docmodel_* builders.
 * Clean-room C11. */

#include "readers.h"
#include "../wubuedit/docmodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    dm_doc *doc;
    /* current paragraph accumulation */
    char  *para; size_t para_n, para_cap;
    int    bold;
    /* table row accumulation */
    dm_para **rowcells; size_t rown, rowcap;
    int    in_table;          /* inside a \trowd..\row block */
} rtf_state;

static void para_reset(rtf_state *s) { s->para_n = 0; if (s->para) s->para[0] = '\0'; s->bold = 0; }

static void para_append(rtf_state *s, const char *src, size_t n) {
    if (!n) return;
    if (s->para_n + n + 1 > s->para_cap) {
        size_t nc = s->para_cap ? s->para_cap * 2 : 64;
        while (s->para_n + n + 1 > nc) nc *= 2;
        s->para = realloc(s->para, nc); s->para_cap = nc;
    }
    memcpy(s->para + s->para_n, src, n);
    s->para_n += n; s->para[s->para_n] = '\0';
}

static void para_flush(rtf_state *s) {
    if (s->para_n == 0 && !s->in_table) {
        /* still emit an empty paragraph so structure is preserved? skip empties */
        return;
    }
    wubuedit_docmodel_add_para(s->doc, NULL, s->bold, s->para ? s->para : "");
    para_reset(s);
}

static void row_flush(rtf_state *s) {
    if (s->rown == 0) { s->in_table = 0; return; }
    wubuedit_docmodel_add_table(s->doc, s->rowcells, 1, s->rown);
    s->rowcells = NULL; s->rown = s->rowcap = 0;
    s->in_table = 0;
}

/* Emit the accumulated paragraph as one cell of the current row. */
static void cell_flush(rtf_state *s) {
    dm_para *cp = calloc(1, sizeof *cp);
    cp->text = s->para ? strdup(s->para) : strdup("");
    cp->bold = s->bold;
    if (s->rown == s->rowcap) {
        size_t nc = s->rowcap ? s->rowcap * 2 : 4;
        s->rowcells = realloc(s->rowcells, nc * sizeof *s->rowcells); s->rowcap = nc;
    }
    s->rowcells[s->rown++] = cp;
    para_reset(s);
}

int wuburead_rtf(const char *path, dm_doc *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz ? (size_t)sz + 1 : 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return -1; }
    fclose(f);
    buf[sz] = '\0';

    rtf_state s; memset(&s, 0, sizeof s); s.doc = out;

    /* skip the {\rtf1 header up to the first non-keyword content */
    const char *p = buf;
    /* find first '{' */
    while (*p && *p != '{') p++;
    int group_depth = 0;
    int skip_group = 0;          /* ignore destination groups like {\*\...} */
    while (*p) {
        if (*p == '{') {
            group_depth++;
            /* destination group {\*...}: skip its content */
            if (p[1] == '\\' && p[2] == '*') skip_group++;
            p++; continue;
        }
        if (*p == '}') {
            if (skip_group) { skip_group--; p++; continue; }
            if (s.in_table) row_flush(&s);
            else para_flush(&s);
            group_depth--; p++; continue;
        }
        if (*p == '\\') {
            /* control word / symbol */
            p++;
            if (*p == '\\' || *p == '{' || *p == '}' || *p == '~' || *p == '-' || *p == '_') {
                if (!skip_group) para_append(&s, p, 1);
                p++; continue;
            }
            /* read control word name */
            char cw[32]; int ci = 0;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) {
                if (ci < 31) { cw[ci++] = *p; }
                p++;
            }
            cw[ci] = '\0';
            /* optional numeric parameter */
            int param = 0, has_param = 0;
            if (*p == '-' || (*p >= '0' && *p <= '9')) {
                has_param = 1; param = strtol(p, (char **)&p, 10);
            } else if (*p == ' ') {
                p++; /* space terminates control word, consumed */
            }
            if (skip_group) continue;
            if (strcmp(cw, "par") == 0 || strcmp(cw, "pard") == 0) {
                if (s.in_table) cell_flush(&s); else para_flush(&s);
            } else if (strcmp(cw, "b") == 0) {
                s.bold = (!has_param || param != 0);
            } else if (strcmp(cw, "trowd") == 0) {
                s.in_table = 1;
            } else if (strcmp(cw, "cell") == 0) {
                if (s.in_table) cell_flush(&s);
            } else if (strcmp(cw, "row") == 0) {
                if (s.in_table) row_flush(&s);
            } else if (strcmp(cw, "line") == 0 || strcmp(cw, "tab") == 0) {
                para_append(&s, "\t", 1);
            }
            /* other control words (fonts, colors, etc.) ignored */
            continue;
        }
        /* plain text */
        if (!skip_group && *p != '\r' && *p != '\n') para_append(&s, p, 1);
        p++;
    }
    /* flush trailing */
    if (s.in_table) row_flush(&s);
    else if (s.para_n) para_flush(&s);

    free(s.para); free(s.rowcells);
    free(buf);
    return 0;
}
