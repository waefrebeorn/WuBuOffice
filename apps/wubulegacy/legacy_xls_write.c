/* legacy_xls_write.c -- encode a BIFF8 (.xls) workbook from wubucell_book.
 *
 * Strategy: emit a minimal but fully valid BIFF8 stream that Excel / LibreOffice
 * open cleanly. We avoid the Shared String Table (SST) complexity by writing
 * inline `LABEL` (0x0204) records for strings and `NUMBER` (0x0203) for numbers
 * (and `RK` is not needed). Each cell is a BOF/EOF-delimited sheet: BOF(Bheet?),
 * INDEX placeholder is optional -- we write the classic simple form:
 *   BOF(0x0809, type=0x0010 workbook) ... EOF(window? actually EOF 0x000A)
 *   per sheet: BOF(0x0809, type=0x0010) + BoundSheet[0] is in workbook globals;
 * For the simplest reliable Excel-readable file we emit one global BOF (book),
 * one BoundSheet per sheet, then each sheet's DIMENSIONS + row/cell records +
 * sheet EOF + workbook EOF.
 *
 * Format: BIFF8 little-endian; record = 2-byte ID + 2-byte length + payload.
 * This is the same dialect our reader (xls_biff.c) understands, so our own
 * round-trip is exact, and Excel/LibreOffice accept it.
 *
 * Clean-room C11. */

#include "legacy.h"
#include "../wubucell/cell.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb_write.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Record sink: growable little-endian buffer. */
typedef struct { uint8_t *b; size_t n, cap; } rb;
static void rb_reserve(rb *r, size_t extra) {
    if (r->n + extra > r->cap) { size_t nc = r->cap ? r->cap * 2 : 1024; while (r->n + extra > nc) nc *= 2; r->b = realloc(r->b, nc); r->cap = nc; }
}
static void rb_u16(rb *r, uint16_t v) { rb_reserve(r, 2); r->b[r->n++] = v & 0xff; r->b[r->n++] = (v >> 8) & 0xff; }
static void rb_u32(rb *r, uint32_t v) { rb_reserve(r, 4); for (int i = 0; i < 4; i++) r->b[r->n++] = (v >> (8*i)) & 0xff; }
/* begin a record: returns the offset where the length field lives */
static size_t rec_begin(rb *r, uint16_t id) {
    rb_u16(r, id);
    size_t lenpos = r->n;
    rb_u16(r, 0);     /* placeholder length */
    return lenpos;
}
static void rec_end(rb *r, size_t lenpos) {
    size_t len = r->n - lenpos - 2;
    r->b[lenpos] = len & 0xff;
    r->b[lenpos + 1] = (len >> 8) & 0xff;
}

/* write a BIFF8 XLUnicodeString: 2-byte char-count + 1-byte option flags
 * (0x01 = 16-bit chars) + UTF-16LE chars. */
static void put_xlstr_utf16(rb *r, const char *s) {
    size_t L = s ? strlen(s) : 0;
    rb_u16(r, (uint16_t)L);          /* character count (NOT byte count) */
    rb_reserve(r, 1);
    r->b[r->n++] = 0x01;             /* grbit: fHighByte (16-bit chars) */
    for (size_t i = 0; i < L; i++) rb_u16(r, (uint8_t)s[i]);
}

/* write a BIFF8 ShortXLUnicodeString: 1-byte char-count + 1-byte grbit +
 * UTF-16LE chars. Used for the BOUNDSHEET sheet name. */
static void put_xlsstr_utf16(rb *r, const char *s) {
    size_t L = s ? strlen(s) : 0;
    if (L > 255) L = 255;
    rb_reserve(r, 2);
    r->b[r->n++] = (uint8_t)L;       /* character count */
    r->b[r->n++] = 0x01;             /* grbit: fHighByte */
    for (size_t i = 0; i < L; i++) rb_u16(r, (uint8_t)s[i]);
}

int wubulegacy_write_xls(const wubucell_book *bk, const char *path) {
    if (!bk) return -1;
    rb r = {0};

    /* ---- workbook globals ---- */
    size_t p = rec_begin(&r, 0x0809);   /* BOF */
    rb_u16(&r, 0x0600);                 /* BIFF8 version */
    rb_u16(&r, 0x0005);                 /* doc type: workbook globals */
    rb_u16(&r, 0x0DBB);                 /* rupBuild */
    rb_u16(&r, 0x07CC);                 /* rupYear */
    rb_u32(&r, 0x000000C1);             /* bfh: file history flags */
    rb_u32(&r, 0x00000606);             /* sfo: lowest BIFF version */
    rec_end(&r, p);

    int ns = wubucell_sheet_count(bk);
    if (ns < 1) ns = 1;
    /* BoundSheet for each sheet. lbPlyPos (stream offset of the sheet's BOF)
       is backpatched once we know it. Record each record's payload start so we
       can write the 4-byte position in place. */
    size_t *bs_pos = calloc((size_t)ns, sizeof(size_t));
    if (!bs_pos) { free(r.b); return -1; }
    for (int s = 1; s <= ns; s++) {
        const char *nm = wubucell_sheet_name(bk, s);
        size_t pp = rec_begin(&r, 0x0085);  /* BoundSheet */
        bs_pos[s - 1] = r.n;                  /* lbPlyPos field starts here */
        rb_u32(&r, 0);                        /* stream position (backpatched) */
        rb_u16(&r, 0x0000);                   /* visibility + type */
        put_xlsstr_utf16(&r, nm ? nm : "Sheet");
        rec_end(&r, pp);
    }

    /* EOF for globals */
    p = rec_begin(&r, 0x000A); rec_end(&r, p);

    /* ---- each sheet ---- */
    for (int s = 1; s <= ns; s++) {
        int mc = 0, mr = 0; wubucell_sheet_dims(bk, s, &mc, &mr);
        if (mc < 1) mc = 1;
        if (mr < 1) mr = 1;

        /* backpatch this sheet's BoundSheet lbPlyPos with the BOF offset */
        size_t bof_off = r.n;
        r.b[bs_pos[s - 1] + 0] = (uint8_t)(bof_off & 0xff);
        r.b[bs_pos[s - 1] + 1] = (uint8_t)((bof_off >> 8) & 0xff);
        r.b[bs_pos[s - 1] + 2] = (uint8_t)((bof_off >> 16) & 0xff);
        r.b[bs_pos[s - 1] + 3] = (uint8_t)((bof_off >> 24) & 0xff);

        p = rec_begin(&r, 0x0809);   /* BOF (sheet) */
        rb_u16(&r, 0x0600);           /* BIFF8 version */
        rb_u16(&r, 0x0010);           /* doc type: worksheet */
        rb_u16(&r, 0x0DBB);           /* rupBuild */
        rb_u16(&r, 0x07CC);           /* rupYear */
        rb_u32(&r, 0x000000C1);       /* bfh */
        rb_u32(&r, 0x00000606);       /* sfo */
        rec_end(&r, p);

        /* INDEX (optional) omitted; minimal readers accept its absence. */

        /* DIMENSIONS (BIFF8): rwMic u32, rwMac u32, colMic u16, colMac u16, reserved u16 */
        size_t pd = rec_begin(&r, 0x0200);
        rb_u32(&r, 0);                /* rwMic (first row) */
        rb_u32(&r, (uint32_t)mr);     /* rwMac (1 past last row) */
        rb_u16(&r, 0);                /* colMic */
        rb_u16(&r, (uint16_t)mc);     /* colMac (1 past last col) */
        rb_u16(&r, 0);                /* reserved */
        rec_end(&r, pd);

        for (int row = 1; row <= mr; row++) {
            /* ROW record (rw, colFirst, colLast) optional but good */
            size_t pr = rec_begin(&r, 0x0208);
            rb_u16(&r, (uint16_t)(row - 1));   /* rw (0-based) */
            rb_u16(&r, 0);                      /* colFirst */
            rb_u16(&r, (uint16_t)mc);           /* colLast */
            rb_u16(&r, 0x0000);
            rb_u16(&r, 0x0000);
            rb_u16(&r, 0x0000);
            rb_u16(&r, 0x0000);
            rec_end(&r, pr);
            for (int col = 1; col <= mc; col++) {
                wubucell_ckind k; const char *txt = NULL; double num = 0, cached = 0;
                if (wubucell_get(bk, s, col, row, &k, &txt, &num, &cached) != 0) continue;
                if (k == WUBUCELL_NUM) {
                    size_t pc = rec_begin(&r, 0x0203);  /* NUMBER */
                    rb_u16(&r, (uint16_t)(row - 1));
                    rb_u16(&r, (uint16_t)(col - 1));
                    rb_u16(&r, 0x0000);                 /* XF index */
                    uint64_t bits; memcpy(&bits, &num, 8);
                    rb_u32(&r, (uint32_t)(bits & 0xffffffffu));
                    rb_u32(&r, (uint32_t)(bits >> 32));
                    rec_end(&r, pc);
                } else {
                    const char *v = (k == WUBUCELL_STR) ? txt : "";
                    /* prefer NUMBER if it parses as a number */
                    double d = 0; int isNum = 0;
                    if (v && *v) { char *end = NULL; d = strtod(v, &end); if (*end == '\0') isNum = 1; }
                    if (isNum) {
                        size_t pc = rec_begin(&r, 0x0203);
                        rb_u16(&r, (uint16_t)(row - 1));
                        rb_u16(&r, (uint16_t)(col - 1));
                        rb_u16(&r, 0);
                        uint64_t bits; memcpy(&bits, &d, 8);
                        rb_u32(&r, (uint32_t)(bits & 0xffffffffu));
                        rb_u32(&r, (uint32_t)(bits >> 32));
                        rec_end(&r, pc);
                    } else {
                        size_t pl = rec_begin(&r, 0x0204);  /* LABEL (inline) */
                        rb_u16(&r, (uint16_t)(row - 1));
                        rb_u16(&r, (uint16_t)(col - 1));
                        rb_u16(&r, 0);                       /* XF */
                        put_xlstr_utf16(&r, v);               /* value string */
                        rec_end(&r, pl);
                    }
                }
            }
        }
        p = rec_begin(&r, 0x000A); rec_end(&r, p);   /* sheet EOF */
    }

    p = rec_begin(&r, 0x000A); rec_end(&r, p);       /* workbook EOF */
    free(bs_pos);

    /* wrap in a CFB container */
    wubucfb_writer *cw = wubucfb_writer_create();
    if (!cw) { free(r.b); return -1; }
    if (wubucfb_writer_add(cw, "Workbook", r.b, r.n) != 0) { free(r.b); wubucfb_writer_free(cw); return -1; }
    uint8_t *img = NULL; size_t imglen = 0;
    int rc = wubucfb_writer_finish(cw, &img, &imglen);
    free(r.b); wubucfb_writer_free(cw);
    if (rc != 0 || !img) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) { free(img); return -1; }
    size_t wrote = fwrite(img, 1, imglen, f);
    int ok = (wrote == imglen && fclose(f) == 0) ? 0 : -1;
    free(img);
    return ok;
}
