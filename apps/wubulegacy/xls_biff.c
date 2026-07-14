/* xls_biff.c -- legacy Excel (.xls, BIFF8) reader -> wubucell_book.
 *
 * A .xls is a CFB container with a "Workbook" (or older "Book") stream of
 * BIFF8 records: [type u16][len u16][data...]. We decode the substream
 * structure (globals + one substream per worksheet), the Shared String Table
 * (SST, which can span CONTINUE records and split strings mid-field), and the
 * common cell records: LABELSST, LABEL, NUMBER, RK, MULRK, FORMULA.
 *
 * Clean-room C11. Read-only. */

#include "legacy.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb.h"

#include <string.h>

/* BIFF record types we care about. */
#define R_BOF        0x0809
#define R_EOF        0x000A
#define R_BOUNDSHEET 0x0085
#define R_SST        0x00FC
#define R_CONTINUE   0x003C
#define R_LABELSST   0x00FD
#define R_LABEL      0x0204
#define R_NUMBER     0x0203
#define R_RK         0x027E
#define R_MULRK      0x00BD
#define R_FORMULA    0x0006
#define R_STRING     0x0207   /* cached string result for a FORMULA */

/* ---- SST segment cursor: reads across the SST record and its CONTINUEs ---- */
typedef struct {
    const uint8_t *base;   /* whole Workbook stream */
    size_t         len;
    size_t         pos;    /* absolute offset of the current byte to read */
    size_t         seg_end;/* absolute end of the current record's payload */
} sst_cursor;

/* Advance to the next CONTINUE record payload when the current segment is
 * exhausted. Returns 1 if positioned on more data, 0 if no CONTINUE follows. */
static int sst_next_segment(sst_cursor *sc) {
    /* the record header sits right at seg_end: [type u16][len u16] */
    if (sc->seg_end + 4 > sc->len) return 0;
    uint16_t type = lg_rd16(sc->base + sc->seg_end);
    if (type != R_CONTINUE) return 0;
    uint16_t rlen = lg_rd16(sc->base + sc->seg_end + 2);
    sc->pos = sc->seg_end + 4;
    sc->seg_end = sc->pos + rlen;
    if (sc->seg_end > sc->len) sc->seg_end = sc->len;
    return 1;
}

/* Bytes left in the current segment. */
static size_t sst_seg_avail(const sst_cursor *sc) {
    return sc->pos < sc->seg_end ? sc->seg_end - sc->pos : 0;
}

/* Skip `n` bytes of run/phonetic data, crossing CONTINUE boundaries. */
static void sst_skip(sst_cursor *sc, size_t n) {
    while (n) {
        size_t avail = sst_seg_avail(sc);
        if (avail == 0) { if (!sst_next_segment(sc)) return; avail = sst_seg_avail(sc); if (!avail) return; }
        size_t take = n < avail ? n : avail;
        sc->pos += take; n -= take;
    }
}

/* Parse one XLUnicodeRichExtendedString at the cursor into a fresh UTF-8
 * string (caller frees). Handles the grbit that re-appears after a mid-string
 * CONTINUE split. Returns NULL on error. */
static char *sst_read_string(sst_cursor *sc) {
    if (sst_seg_avail(sc) < 3 && !sst_next_segment(sc)) {
        /* header must not be split; but if the segment is short, refill */
    }
    /* cChars (u16) + grbit (u8) are guaranteed not to be split across a
     * CONTINUE boundary by the spec, so read them from the current segment. */
    if (sst_seg_avail(sc) < 3) return NULL;
    uint16_t cch = lg_rd16(sc->base + sc->pos); sc->pos += 2;
    uint8_t grbit = sc->base[sc->pos++];
    int fHigh = grbit & 0x01;
    int fExt  = grbit & 0x04;
    int fRich = grbit & 0x08;
    uint16_t cRun = 0;
    uint32_t cbExt = 0;
    if (fRich) { cRun = lg_rd16(sc->base + sc->pos); sc->pos += 2; }
    if (fExt)  { cbExt = lg_rd32(sc->base + sc->pos); sc->pos += 4; }

    char *out = malloc((size_t)cch * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    size_t remaining = cch;
    int cur_high = fHigh;
    while (remaining > 0) {
        if (sst_seg_avail(sc) == 0) {
            if (!sst_next_segment(sc)) break;
            /* after a split, a fresh grbit option byte precedes the rest */
            int gb = sc->base[sc->pos++];
            cur_high = gb & 0x01;
        }
        size_t avail = sst_seg_avail(sc);
        size_t can = cur_high ? avail / 2 : avail;
        if (can == 0) { /* one dangling byte of a 2-byte char: force refill */
            if (!sst_next_segment(sc)) break;
            int gb = sc->base[sc->pos++];
            cur_high = gb & 0x01;
            continue;
        }
        size_t take = remaining < can ? remaining : can;
        for (size_t i = 0; i < take; i++) {
            uint32_t cpv = cur_high ? lg_rd16(sc->base + sc->pos) : sc->base[sc->pos];
            sc->pos += cur_high ? 2 : 1;
            lg_put_utf8(cpv, &p);
        }
        remaining -= take;
    }
    *p = '\0';

    /* skip rich-text runs (4 bytes each) and extended phonetic data */
    if (fRich) sst_skip(sc, (size_t)cRun * 4);
    if (fExt)  sst_skip(sc, cbExt);
    return out;
}

/* Decode a BIFF RK value into a double. */
static double rk_to_double(uint32_t rk) {
    double v;
    if (rk & 0x02) {
        v = (double)((int32_t)rk >> 2);      /* fInt: signed 30-bit int */
    } else {
        uint64_t bits = ((uint64_t)(rk & 0xFFFFFFFC)) << 32;
        memcpy(&v, &bits, sizeof v);
    }
    if (rk & 0x01) v /= 100.0;               /* fx100 */
    return v;
}

/* ---- BOUNDSHEET name list ---- */
typedef struct { char *name; } xls_sheet;

int wubulegacy_read_xls(const char *path, wubucell_book **out) {
    if (!out) return -1;
    *out = NULL;

    size_t flen = 0;
    uint8_t *file = lg_slurp(path, &flen);
    if (!file) return -1;

    wubucfb *c = wubucfb_open(file, flen);
    free(file);
    if (!c) return -1;

    uint8_t *wb = NULL; size_t wblen = 0;
    if (wubucfb_read_stream(c, "Workbook", &wb, &wblen) != 0 &&
        wubucfb_read_stream(c, "Book", &wb, &wblen) != 0) {
        wubucfb_close(c);
        return -1;
    }
    wubucfb_close(c);

    /* --- pass over records --- */
    char **sst = NULL; size_t sst_n = 0;
    xls_sheet *sheets = NULL; size_t nsheets = 0;
    wubucell_book *book = wubucell_create();
    if (!book) { free(wb); return -1; }

    int cur_sheet = 0;      /* 1-based wubucell sheet, 0 = none yet */
    size_t sheet_seen = 0;  /* worksheet BOFs consumed so far */

    size_t pos = 0;
    while (pos + 4 <= wblen) {
        uint16_t type = lg_rd16(wb + pos);
        uint16_t len  = lg_rd16(wb + pos + 2);
        size_t data = pos + 4;
        if (data + len > wblen) break;
        const uint8_t *d = wb + data;

        switch (type) {
        case R_BOUNDSHEET: {
            /* [lbPlyPos u32][visibility u8][type u8][name: ShortXLUnicode] */
            if (len >= 8) {
                uint8_t cch = d[6];
                uint8_t gr  = d[7];
                int fHigh = gr & 0x01;
                char *nm = lg_u16_to_utf8(d + 8, cch, fHigh);
                xls_sheet *ns = realloc(sheets, (nsheets + 1) * sizeof *sheets);
                if (ns) { sheets = ns; sheets[nsheets++].name = nm; }
                else free(nm);
            }
            break;
        }
        case R_SST: {
            /* [cstTotal u32][cstUnique u32][strings...] spanning CONTINUEs */
            if (len >= 8) {
                uint32_t unique = lg_rd32(d + 4);
                sst_cursor sc = { wb, wblen, data + 8, data + len };
                sst = calloc(unique ? unique : 1, sizeof(char *));
                if (sst) {
                    for (uint32_t i = 0; i < unique; i++) {
                        char *s = sst_read_string(&sc);
                        if (!s) break;
                        sst[sst_n++] = s;
                    }
                }
                /* jump the outer loop past the SST + its CONTINUE records */
                pos = sc.seg_end;
                continue;
            }
            break;
        }
        case R_BOF: {
            if (len >= 4) {
                uint16_t dt = lg_rd16(d + 2);
                if (dt == 0x0010) {   /* worksheet substream */
                    const char *nm = (sheet_seen < nsheets && sheets[sheet_seen].name)
                                     ? sheets[sheet_seen].name : "Sheet";
                    cur_sheet = wubucell_sheet(book, nm);
                    sheet_seen++;
                }
            }
            break;
        }
        case R_LABELSST: {
            if (len >= 10 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t col = lg_rd16(d + 2);
                uint32_t isst = lg_rd32(d + 6);
                const char *s = (isst < sst_n) ? sst[isst] : "";
                wubucell_cell_s(book, cur_sheet, col + 1, row + 1, s ? s : "");
            }
            break;
        }
        case R_LABEL: {
            if (len >= 8 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t col = lg_rd16(d + 2);
                uint16_t cch = lg_rd16(d + 6);
                if (len >= 9) {
                    uint8_t gr = d[8];
                    char *s = lg_u16_to_utf8(d + 9, cch, gr & 0x01);
                    if (s) { wubucell_cell_s(book, cur_sheet, col + 1, row + 1, s); free(s); }
                }
            }
            break;
        }
        case R_NUMBER: {
            if (len >= 14 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t col = lg_rd16(d + 2);
                double v; memcpy(&v, d + 6, 8);
                wubucell_cell_n(book, cur_sheet, col + 1, row + 1, v);
            }
            break;
        }
        case R_RK: {
            if (len >= 10 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t col = lg_rd16(d + 2);
                double v = rk_to_double(lg_rd32(d + 6));
                wubucell_cell_n(book, cur_sheet, col + 1, row + 1, v);
            }
            break;
        }
        case R_MULRK: {
            /* [row u16][colFirst u16] N*[xf u16][rk u32] [colLast u16] */
            if (len >= 6 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t colf = lg_rd16(d + 2);
                uint16_t coll = lg_rd16(d + len - 2);
                size_t n = (coll >= colf) ? (size_t)(coll - colf + 1) : 0;
                for (size_t i = 0; i < n; i++) {
                    size_t off = 4 + i * 6;
                    if (off + 6 > (size_t)len) break;
                    double v = rk_to_double(lg_rd32(d + off + 2));
                    wubucell_cell_n(book, cur_sheet, colf + (int)i + 1, row + 1, v);
                }
            }
            break;
        }
        case R_FORMULA: {
            /* [row u16][col u16][xf u16][result 8 bytes][...]. If the result
             * encodes a string, the value follows in a STRING record; here we
             * store the numeric cached result when the result is a number. */
            if (len >= 20 && cur_sheet) {
                uint16_t row = lg_rd16(d);
                uint16_t col = lg_rd16(d + 2);
                /* result is a number unless bytes[6..7]==0xFFFF sentinel */
                if (!(d[12] == 0xFF && d[13] == 0xFF)) {
                    double v; memcpy(&v, d + 6, 8);
                    wubucell_cell_n(book, cur_sheet, col + 1, row + 1, v);
                }
            }
            break;
        }
        default: break;
        }

        pos = data + len;
    }

    free(wb);
    for (size_t i = 0; i < sst_n; i++) free(sst[i]);
    free(sst);
    for (size_t i = 0; i < nsheets; i++) free(sheets[i].name);
    free(sheets);

    *out = book;
    return 0;
}
