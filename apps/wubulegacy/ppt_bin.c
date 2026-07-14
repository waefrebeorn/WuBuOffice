/* ppt_bin.c -- legacy PowerPoint (.ppt) reader -> wubushow_pres.
 *
 * A .ppt is a CFB container with a "PowerPoint Document" stream of records:
 *   [recVerAndInstance u16][recType u16][recLen u32][data...]
 * where the low nibble of recVerAndInstance == 0xF marks a container whose
 * data is itself a sequence of records. Slide text lives in TextCharsAtom
 * (0x0FA0, UTF-16LE) and TextBytesAtom (0x0FA8, CP1252) leaf records, grouped
 * under Slide containers (0x03EE). We recurse the tree, and for each Slide
 * container collect its text atoms: the first becomes the title, the rest the
 * body (one bullet per atom, newline-joined).
 *
 * Clean-room C11. Read-only, text extraction. */

#include "legacy.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb.h"

#include <string.h>

#define REC_SLIDE          0x03EE
#define REC_TEXTCHARS      0x0FA0
#define REC_TEXTBYTES      0x0FA8

/* CP1252 specials (subset shared with doc_bin logic, kept local + minimal). */
static uint32_t ppt_cp1252(uint8_t b) {
    switch (b) {
        case 0x91: return 0x2018; case 0x92: return 0x2019;
        case 0x93: return 0x201C; case 0x94: return 0x201D;
        case 0x95: return 0x2022; case 0x96: return 0x2013;
        case 0x97: return 0x2014; case 0x85: return 0x2026;
        default:   return b;
    }
}

/* Collector for one slide's text atoms. */
typedef struct {
    char  **atoms;
    size_t  n, cap;
} slide_text;

static void st_add(slide_text *st, char *s) {
    if (!s) return;
    if (st->n + 1 > st->cap) {
        st->cap = st->cap ? st->cap * 2 : 4;
        st->atoms = realloc(st->atoms, st->cap * sizeof(char *));
    }
    st->atoms[st->n++] = s;
}

/* Convert a run of PowerPoint text to UTF-8, normalizing the vertical-tab
 * (0x0B) soft break and carriage return (0x0D) to '\n'. `high`=1 for UTF-16. */
static char *ppt_text_utf8(const uint8_t *d, size_t bytes, int high) {
    size_t nchars = high ? bytes / 2 : bytes;
    char *out = malloc(nchars * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    for (size_t i = 0; i < nchars; i++) {
        uint32_t cp = high ? lg_rd16(d + i * 2) : ppt_cp1252(d[i]);
        if (cp == 0x0B || cp == 0x0D) cp = '\n';
        if (cp == 0x00) continue;
        lg_put_utf8(cp, &p);
    }
    *p = '\0';
    return out;
}

/* Recursively walk records in [d, d+len). When inside a Slide container,
 * `st` is non-NULL and text atoms are appended to it. `nslides` counts Slide
 * containers seen; `all` collects every text atom (used as a fallback when the
 * file carries no Slide containers, e.g. master-only decks). */
static void walk(const uint8_t *d, size_t len, slide_text *st, wubushow_pres *pres,
                 int depth, int *nslides, slide_text *all) {
    if (depth > 32) return;
    size_t p = 0;
    while (p + 8 <= len) {
        uint16_t ver = lg_rd16(d + p);
        uint16_t type = lg_rd16(d + p + 2);
        uint32_t rlen = lg_rd32(d + p + 4);
        size_t body = p + 8;
        if (body + rlen > len) rlen = (uint32_t)(len - body);
        const uint8_t *rd = d + body;

        int is_container = (ver & 0x000F) == 0x000F;

        if (type == REC_SLIDE) {
            if (nslides) (*nslides)++;
            /* open a new slide: collect its text, then flush */
            slide_text local = {0};
            walk(rd, rlen, &local, pres, depth + 1, NULL, all);
            const char *title = local.n > 0 ? local.atoms[0] : "";
            /* body = atoms[1..] joined by newline */
            size_t bl = 0;
            for (size_t i = 1; i < local.n; i++) bl += strlen(local.atoms[i]) + 1;
            char *bodybuf = malloc(bl + 1);
            bodybuf[0] = '\0';
            size_t off = 0;
            for (size_t i = 1; i < local.n; i++) {
                size_t L = strlen(local.atoms[i]);
                memcpy(bodybuf + off, local.atoms[i], L); off += L;
                if (i + 1 < local.n) bodybuf[off++] = '\n';
            }
            bodybuf[off] = '\0';
            /* skip a slide that carried no text at all */
            if ((title && title[0]) || (bodybuf && bodybuf[0]))
                wubushow_slide(pres, title, bodybuf);
            free(bodybuf);
            for (size_t i = 0; i < local.n; i++) free(local.atoms[i]);
            free(local.atoms);
        } else if (is_container) {
            walk(rd, rlen, st, pres, depth + 1, nslides, all);
        } else if (type == REC_TEXTCHARS || type == REC_TEXTBYTES) {
            char *s = ppt_text_utf8(rd, rlen, type == REC_TEXTCHARS);
            if (s && s[0]) {
                if (st) st_add(st, s);
                else if (all) { char *dup = strdup(s); st_add(all, dup); free(s); }
                else free(s);
            } else {
                free(s);
            }
        }

        p = body + rlen;
    }
}

int wubulegacy_read_ppt(const char *path, wubushow_pres **out) {
    if (!out) return -1;
    *out = NULL;

    size_t flen = 0;
    uint8_t *file = lg_slurp(path, &flen);
    if (!file) return -1;
    wubucfb *c = wubucfb_open(file, flen);
    free(file);
    if (!c) return -1;

    uint8_t *doc = NULL; size_t doclen = 0;
    if (wubucfb_read_stream(c, "PowerPoint Document", &doc, &doclen) != 0) {
        wubucfb_close(c); return -1;
    }
    wubucfb_close(c);

    wubushow_pres *pres = wubushow_create();
    if (!pres) { free(doc); return -1; }

    int nslides = 0;
    slide_text all = {0};
    walk(doc, doclen, NULL, pres, 0, &nslides, &all);
    free(doc);

    /* Fallback for master-only / empty-slide decks: if we produced no slides
     * with real text but collected text atoms elsewhere (e.g. under masters),
     * synthesize one slide per atom (first = title) so content is never lost. */
    int have_text = 0;
    int sc = wubushow_slide_count(pres);
    for (int i = 0; i < sc; i++) {
        const char *t = NULL, *b = NULL;
        wubushow_slide_get(pres, i, &t, &b);
        if ((t && t[0]) || (b && b[0])) { have_text = 1; break; }
    }
    if (!have_text && all.n > 0) {
        const char *title = all.atoms[0];
        size_t bl = 0;
        for (size_t i = 1; i < all.n; i++) bl += strlen(all.atoms[i]) + 1;
        char *bodybuf = malloc(bl + 1); bodybuf[0] = '\0';
        size_t off = 0;
        for (size_t i = 1; i < all.n; i++) {
            size_t L = strlen(all.atoms[i]);
            memcpy(bodybuf + off, all.atoms[i], L); off += L;
            if (i + 1 < all.n) bodybuf[off++] = '\n';
        }
        bodybuf[off] = '\0';
        wubushow_slide(pres, title, bodybuf);
        free(bodybuf);
    }
    (void)nslides;
    for (size_t i = 0; i < all.n; i++) free(all.atoms[i]);
    free(all.atoms);

    *out = pres;
    return 0;
}
