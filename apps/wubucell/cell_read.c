/* cell_read.c -- read an .xlsx into a wubucell_book.
 *
 * SAX over wubuxml_parse for xl/sharedStrings.xml (build the shared-string
 * table) and each xl/worksheets/sheetN.xml (build rows of cells). One shared
 * handler with a mode flag, no second copy of the extraction logic.
 *
 * Sheet-to-part mapping follows the OOXML spec: xl/workbook.xml lists sheets
 * in tab order with their r:id; xl/_rels/workbook.xml.rels maps each r:id to
 * the worksheet target; we parse exactly those parts into the matching book
 * sheet. This keeps the book's sheet indices aligned with how the workbook
 * was written (so a reader can round-trip a writer), and is robust to extra
 * parts (content-types, rels, styles, charts) that are not worksheets.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "cell_read.h"
#include "cell_internal.h"
#include "reader.h"
#include "parser.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef enum { RD_SS, RD_SHEET } rd_mode;

typedef struct {
    rd_mode mode;
    wubucell_book *book;
    int sheet_idx;          /* current sheet being filled (1-based book index) */
    sst_t *sst;             /* shared-string table built from sharedStrings.xml */

    /* shared-string table build */
    int in_si;              /* inside <si> */
    int in_t;               /* inside <t> (shared-string text run) */
    char *si_run; size_t si_len, si_cap;

    /* sheet cell build */
    int in_c;               /* inside <c> */
    int in_is;              /* inside <is> (inline string) -- valid until </c> */
    int in_f;               /* inside <f> (formula) */
    int in_v;               /* inside <v> (value/cached) */
    char cell_ref[16];      /* r="..." */
    char cell_t[16];        /* t="..." */
    char *fbuf; size_t f_len, f_cap;   /* formula text */
    char *vbuf; size_t v_len, v_cap;   /* value/cached text */
} rd_state;

/* Append `n` bytes to a growable, NUL-terminated string buffer. */
static void rd_pushs(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 32;
        while (*len + n + 1 > nc) nc *= 2;
        char *nb = realloc(*buf, nc);
        if (!nb) return;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, n); *len += n; (*buf)[*len] = '\0';
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    rd_state *st = (rd_state *)user;
    const char *name = info->name;

    if (evt == WUBUXML_EVT_START) {
        if (st->mode == RD_SS) {
            if (strcmp(name, "si") == 0) { st->in_si = 1; st->si_len = 0; }
            else if (strcmp(name, "t") == 0) { st->in_t = 1; }
        } else { /* RD_SHEET */
            if (strcmp(name, "c") == 0) {
                st->in_c = 1;
                st->cell_ref[0] = '\0'; st->cell_t[0] = '\0';
                st->f_len = 0; st->v_len = 0;
                for (int a = 0; a < info->attr_count; a++) {
                    if (strcmp(info->attr_name[a], "r") == 0)
                        snprintf(st->cell_ref, sizeof st->cell_ref, "%s", info->attr_val[a]);
                    else if (strcmp(info->attr_name[a], "t") == 0)
                        snprintf(st->cell_t, sizeof st->cell_t, "%s", info->attr_val[a]);
                }
            } else if (strcmp(name, "f") == 0) { st->in_f = 1; st->f_len = 0; }
            else if (strcmp(name, "v") == 0) { st->in_v = 1; st->v_len = 0; }
            else if (strcmp(name, "is") == 0) { st->in_is = 1; }
            else if (strcmp(name, "mergeCell") == 0) {
                const char *ref = NULL;
                for (int a = 0; a < info->attr_count; a++)
                    if (strcmp(info->attr_name[a], "ref") == 0) { ref = info->attr_val[a]; break; }
                if (ref) {
                    char a1[32], b2[32]; a1[0] = b2[0] = '\0';
                    const char *colon = strchr(ref, ':');
                    if (colon) {
                        size_t k = 0;
                        for (const char *p = ref; p < colon && k < 31; p++) a1[k++] = *p; a1[k] = '\0';
                        k = 0; for (const char *p = colon + 1; *p && k < 31; p++) b2[k++] = *p; b2[k] = '\0';
                        int c0 = 0, r0 = 0, c1 = 0, r1 = 0;
                        const char *p = a1; while (*p && isalpha((unsigned char)*p)) { c0 = c0 * 26 + (toupper((unsigned char)*p) - 'A' + 1); p++; }
                        r0 = atoi(p);
                        p = b2; while (*p && isalpha((unsigned char)*p)) { c1 = c1 * 26 + (toupper((unsigned char)*p) - 'A' + 1); p++; }
                        r1 = atoi(p);
                        wubucell_merge(st->book, st->sheet_idx, c0, r0, c1, r1);
                    }
                }
            }
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_TEXT) {
        if (st->mode == RD_SS) {
            if (st->in_t && st->in_si) rd_pushs(&st->si_run, &st->si_len, &st->si_cap, info->text, info->text_len);
        } else {
            if (st->in_f) rd_pushs(&st->fbuf, &st->f_len, &st->f_cap, info->text, info->text_len);
            else if (st->in_v || st->in_is) rd_pushs(&st->vbuf, &st->v_len, &st->v_cap, info->text, info->text_len);
        }
        return 0;
    }

    /* END */
    if (st->mode == RD_SS) {
        if (strcmp(name, "t") == 0) st->in_t = 0;
        else if (strcmp(name, "si") == 0) {
            cell_sst_add(st->sst, st->si_run ? st->si_run : "");
            st->in_si = 0; st->si_len = 0;
        }
        return 0;
    }

    /* RD_SHEET END */
    if (strcmp(name, "f") == 0) { st->in_f = 0; }
    else if (strcmp(name, "v") == 0) { st->in_v = 0; }
    else if (strcmp(name, "c") == 0) {
        st->in_c = 0;
        /* commit the cell */
        if (st->cell_ref[0]) {
            /* parse ref -> col/row */
            int col = 0, row = 0;
            const char *p = st->cell_ref;
            while (*p && isalpha((unsigned char)*p)) { col = col * 26 + (toupper((unsigned char)*p) - 'A' + 1); p++; }
            row = atoi(p);
            sheet_t *sh = cell_book_sheet(st->book, st->sheet_idx);
            if (sh) {
                int have_v = (st->v_len > 0);   /* <v> present AND non-empty */
                int have_f = (st->f_len > 0);   /* <f> present AND non-empty */
                if (strcmp(st->cell_t, "s") == 0 && have_v) {
                    int idx = atoi(st->vbuf);
                    const char *s = (idx >= 0 && (size_t)idx < st->sst->n) ? st->sst->e[idx].s : "";
                    wubucell_cell_s(st->book, st->sheet_idx, col, row, s);
                } else if (strcmp(st->cell_t, "inlineStr") == 0 || strcmp(st->cell_t, "str") == 0 || st->in_is) {
                    wubucell_cell_s(st->book, st->sheet_idx, col, row, have_v ? st->vbuf : "");
                } else if (have_f) {
                    /* Formula cell. Foreign producers (openpyxl) often omit the
                     * cached <v>; store what we have and let cell_eval_all()
                     * recompute the real result after the whole book loads. */
                    double cached = have_v ? atof(st->vbuf) : 0.0;
                    wubucell_cell_f(st->book, st->sheet_idx, col, row, st->fbuf, cached);
                } else if (have_v) {
                    wubucell_cell_n(st->book, st->sheet_idx, col, row, atof(st->vbuf));
                }
            }
        }
        st->cell_ref[0] = '\0'; st->cell_t[0] = '\0'; st->f_len = 0; st->v_len = 0; st->in_is = 0;
    }
    return 0;
}

/* --- workbook.xml / .rels bookkeeping (minimal, self-contained scans) --- */

typedef struct { char name[64]; char rid[32]; char target[256]; } wb_sheet;
typedef struct { char rid[32]; char target[256]; } wb_rel;

/* Collect ordered <sheet name=".." r:id=".."> entries from workbook.xml. */
static void wb_collect_sheets(const uint8_t *d, size_t n, wb_sheet *out, size_t *cnt, size_t cap) {
    const char *s = (const char *)d;
    *cnt = 0;
    for (size_t i = 0; i + 5 <= n; ) {
        if (strncmp(s + i, "<sheet", 6) != 0) { i++; continue; }
        char name[64] = "", rid[32] = "";
        const char *p = s + i;
        const char *q;
        if ((q = strstr(p, "name=\"")))  { q += 6; size_t j = 0; while (*q && *q != '"' && j < 63) name[j++] = *q++; name[j] = '\0'; }
        /* r:id (or id) attribute -- namespace prefix kept as written */
        if ((q = strstr(p, "r:id=\"")))  { q += 6; size_t j = 0; while (*q && *q != '"' && j < 31) rid[j++] = *q++; rid[j] = '\0'; }
        else if ((q = strstr(p, "r:id='"))) { q += 6; size_t j = 0; while (*q && *q != '\'' && j < 31) rid[j++] = *q++; rid[j] = '\0'; }
        if (name[0] && *cnt < cap) {
            strncpy(out[*cnt].name, name, sizeof out[*cnt].name - 1); out[*cnt].name[sizeof out[*cnt].name - 1] = '\0';
            strncpy(out[*cnt].rid, rid, sizeof out[*cnt].rid - 1); out[*cnt].rid[sizeof out[*cnt].rid - 1] = '\0';
            (*cnt)++;
        }
        const char *end = strstr(p, "/>"); if (!end) end = strstr(p, ">");
        if (!end) break;
        i = (size_t)(end - s) + 1;
    }
}

/* Collect <Relationship Id=".." Target=".."> entries from a .rels part. */
static void rels_collect(const uint8_t *d, size_t n, wb_rel *out, size_t *cnt, size_t cap) {
    const char *s = (const char *)d;
    *cnt = 0;
    for (size_t i = 0; i + 12 <= n; ) {
        if (strncmp(s + i, "<Relationship", 12) != 0) { i++; continue; }
        char rid[32] = "", tgt[256] = "";
        const char *p = s + i;
        const char *q;
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

/* Resolve a workbook r:id to an absolute package part name. */
static const char *resolve_target(const wb_rel *rels, size_t nrel, const char *rid, char *buf, size_t buflen) {
    for (size_t i = 0; i < nrel; i++) {
        if (strcmp(rels[i].rid, rid) == 0) {
            const char *t = rels[i].target;
            if (t[0] == '/') { snprintf(buf, buflen, "%s", t + 1); }
            else if (strncmp(t, "xl/", 3) == 0) { snprintf(buf, buflen, "%s", t); }
            else { snprintf(buf, buflen, "xl/%s", t); } /* relative to xl/ */
            return buf;
        }
    }
    return NULL;
}

int wubucell_read(const char *path, wubucell_book **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(data, (size_t)sz, &pkg) != 0) { free(data); return -1; }

    wubucell_book *b = wubucell_create();
    if (!b) { wubuoxml_free(&pkg); free(data); return -1; }

    /* build shared-string table from xl/sharedStrings.xml if present */
    sst_t sst; memset(&sst, 0, sizeof sst);
    const wubuoxml_part *ss = wubuoxml_part_find(&pkg, "xl/sharedStrings.xml");
    if (ss) {
        rd_state st; memset(&st, 0, sizeof st);
        st.mode = RD_SS; st.book = b; st.sst = &sst;
        wubuxml_parse(ss->bytes, ss->len, on_event, &st);
        free(st.si_run);
    }

    /* enumerate worksheets via workbook.xml + its rels (the spec path) */
    wb_sheet sheets[256]; size_t nsheets = 0;
    wb_rel rels[256];     size_t nrels = 0;
    char tgt[320];

    const wubuoxml_part *wb = wubuoxml_part_find(&pkg, "xl/workbook.xml");
    if (wb) {
        wb_collect_sheets(wb->bytes, wb->len, sheets, &nsheets, 256);
        const wubuoxml_part *wbr = wubuoxml_part_find(&pkg, "xl/_rels/workbook.xml.rels");
        if (wbr) rels_collect(wbr->bytes, wbr->len, rels, &nrels, 256);
    }

    if (nsheets == 0) {
        /* Fallback: no workbook.xml parsed -- scan for worksheets in numeric
         * order (robust for minimal/third-party packages). */
        for (size_t i = 1; i < 1000; i++) {
            snprintf(tgt, sizeof tgt, "xl/worksheets/sheet%zu.xml", i);
            if (!wubuoxml_part_find(&pkg, tgt)) break;
            snprintf(sheets[nsheets].name, sizeof sheets[nsheets].name, "Sheet%zu", i);
            sheets[nsheets].rid[0] = '\0';
            { size_t tl = strlen(tgt);
              if (tl >= sizeof sheets[nsheets].target) tl = sizeof sheets[nsheets].target - 1;
              memcpy(sheets[nsheets].target, tgt, tl);
              sheets[nsheets].target[tl] = '\0'; }
            nsheets++;
        }
    }

    for (size_t i = 0; i < nsheets; i++) {
        const char *partname;
        char resolved[320];
        if (sheets[i].rid[0]) {
            partname = resolve_target(rels, nrels, sheets[i].rid, resolved, sizeof resolved);
        } else {
            /* fallback entry already carries a full part path in `target` */
            partname = sheets[i].target;
        }
        const wubuoxml_part *pt = partname ? wubuoxml_part_find(&pkg, partname) : NULL;
        int sh = wubucell_sheet(b, sheets[i].name);
        if (pt) {
            rd_state st; memset(&st, 0, sizeof st);
            st.mode = RD_SHEET; st.book = b; st.sst = &sst; st.sheet_idx = sh;
            wubuxml_parse(pt->bytes, pt->len, on_event, &st);
            free(st.fbuf); free(st.vbuf);
        }
    }

    /* free shared-string table */
    for (size_t i = 0; i < sst.n; i++) free(sst.e[i].s);
    free(sst.e);

    /* Recompute formula results. Foreign producers (openpyxl, LibreOffice in
     * some modes) omit or blank the cached <v> for formula cells; evaluating
     * through our own engine gives correct values instead of trusting theirs.
     * This is "control of destiny": our read of a formula never depends on the
     * other tool having written a cache. */
    cell_eval_all(b);

    wubuoxml_free(&pkg);
    free(data);
    *out = b;
    return 0;
}
