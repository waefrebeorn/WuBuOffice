/* legacy_doc_write.c -- encode a legacy .doc (Word binary) from dm_doc.
 *
 * Produces a Word-97 (.doc) file that our reader (doc_bin.c), Word, and
 * LibreOffice open. Layout:
 *   WordDocument stream = FIB (>= 0x200 bytes, with the FibRgFcLcb97 block) +
 *                         the document text as 16-bit Unicode starting at fcMin.
 *   0Table stream       = the CLX: a single Pcdt (clxt 0x02) whose PlcPcd holds
 *                         one piece descriptor covering all characters, fc
 *                         pointing at the text in WordDocument (uncompressed,
 *                         i.e. 16-bit Unicode).
 *
 * The reader keys off:
 *   flags   @0x000A  (bit 0x0200 selects 1Table vs 0Table -> we clear it -> 0Table)
 *   fcMin   @0x0018  fcMac @0x001C  (byte range of the text, fallback path)
 *   fcClx   @0x01A2  lcbClx @0x01A6 (the CLX location in the table stream)
 *
 * We write real Unicode pieces (fc without the 0x40000000 compressed bit) so
 * the full BMP round-trips losslessly.
 *
 * Clean-room C11. */

#include "legacy.h"
#include "../wubuedit/docmodel.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb_write.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { uint8_t *b; size_t n, cap; } rb;
static void rb_reserve(rb *r, size_t extra){ if(r->n+extra>r->cap){ size_t nc=r->cap?r->cap*2:1024; while(r->n+extra>nc)nc*=2; r->b=realloc(r->b,nc); r->cap=nc; } }
static void rb_u8(rb *r, uint8_t v){ rb_reserve(r,1); r->b[r->n++]=v; }
static void rb_u16(rb *r, uint16_t v){ rb_reserve(r,2); r->b[r->n++]=v&0xff; r->b[r->n++]=(v>>8)&0xff; }
static void rb_u32(rb *r, uint32_t v){ rb_reserve(r,4); for(int i=0;i<4;i++) r->b[r->n++]=(v>>(8*i))&0xff; }
static void rb_bytes(rb *r, const void*p, size_t n){ rb_reserve(r,n); memcpy(r->b+r->n,p,n); r->n+=n; }

/* Decode one UTF-8 code point from p (advancing *p). Returns 0xFFFD on error. */
static uint32_t utf8_next(const char **pp, const char *end) {
    const unsigned char *p = (const unsigned char *)*pp;
    if ((const char *)p >= end) return 0;
    uint32_t cp; int n;
    if (p[0] < 0x80) { cp = p[0]; n = 1; }
    else if ((p[0] & 0xE0) == 0xC0) { cp = p[0] & 0x1F; n = 2; }
    else if ((p[0] & 0xF0) == 0xE0) { cp = p[0] & 0x0F; n = 3; }
    else if ((p[0] & 0xF8) == 0xF0) { cp = p[0] & 0x07; n = 4; }
    else { *pp = (const char *)(p + 1); return 0xFFFD; }
    if ((const char *)(p + n) > end) { *pp = end; return 0xFFFD; }
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *pp = (const char *)(p + 1); return 0xFFFD; }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *pp = (const char *)(p + n);
    return cp;
}

/* Append a paragraph's UTF-8 text as 16-bit Unicode into the text buffer,
 * then a Word paragraph mark (CR = 0x000D). Returns chars written (incl. CR). */
static uint32_t emit_para_u16(rb *text, const char *utf8) {
    uint32_t nch = 0;
    const char *p = utf8 ? utf8 : "";
    const char *end = p + strlen(p);
    while (p < end) {
        uint32_t cp = utf8_next(&p, end);
        if (cp == 0) break;
        if (cp > 0xFFFF) cp = 0xFFFD;      /* BMP only for the 16-bit path */
        if (cp == '\n') cp = 0x000B;       /* soft line break inside a paragraph */
        rb_u16(text, (uint16_t)cp);
        nch++;
    }
    rb_u16(text, 0x000D);                   /* paragraph mark */
    nch++;
    return nch;
}

int wubulegacy_write_doc(const dm_doc *d, const char *path) {
    if (!d) return -1;

    /* ---- build the 16-bit text buffer ---- */
    rb text = {0};
    uint32_t total_cp = 0;
    for (size_t i = 0; i < d->n; i++) {
        const dm_block *bl = &d->blocks[i];
        if (bl->kind == DM_BLOCK_PARA) {
            total_cp += emit_para_u16(&text, bl->para.text ? bl->para.text : "");
        } else {
            for (size_t rr = 0; rr < bl->table.rows; rr++) {
                rb line = {0};
                for (size_t cc = 0; cc < bl->table.cols; cc++) {
                    dm_para *cell = bl->table.cells[rr * bl->table.cols + cc];
                    const char *v = (cell && cell->text) ? cell->text : "";
                    /* join cells with a tab; build a UTF-8 line then emit */
                    rb_bytes(&line, v, strlen(v));
                    if (cc + 1 < bl->table.cols) rb_u8(&line, '\t');
                }
                rb_u8(&line, 0);
                total_cp += emit_para_u16(&text, (const char *)line.b);
                free(line.b);
            }
        }
    }
    if (total_cp == 0) { total_cp = emit_para_u16(&text, ""); }

    /* ---- WordDocument stream: FIB (0x200) + text ---- */
    rb wd = {0};
    /* zero-fill a 0x200-byte FIB, then patch fields */
    for (size_t i = 0; i < 0x200; i++) rb_u8(&wd, 0);
    size_t fc_min = wd.n;                 /* text starts right after the FIB */
    rb_bytes(&wd, text.b, text.n);
    size_t fc_mac = wd.n;

    /* FIB base fields */
    wd.b[0x00] = 0xEC; wd.b[0x01] = 0xA5;      /* wIdent 0xA5EC */
    wd.b[0x02] = 0xC1; wd.b[0x03] = 0x00;      /* nFib 0x00C1 (Word 97) */
    /* flags @0x000A: clear fComplex (0x0004) and fWhichTblStm (0x0200) -> 0Table */
    wd.b[0x0A] = 0x00; wd.b[0x0B] = 0x00;
    /* fcMin @0x0018, fcMac @0x001C (32-bit) */
    wd.b[0x18] = (uint8_t)(fc_min);       wd.b[0x19] = (uint8_t)(fc_min >> 8);
    wd.b[0x1A] = (uint8_t)(fc_min >> 16); wd.b[0x1B] = (uint8_t)(fc_min >> 24);
    wd.b[0x1C] = (uint8_t)(fc_mac);       wd.b[0x1D] = (uint8_t)(fc_mac >> 8);
    wd.b[0x1E] = (uint8_t)(fc_mac >> 16); wd.b[0x1F] = (uint8_t)(fc_mac >> 24);

    /* ---- 0Table stream: the CLX (Pcdt with one piece descriptor) ---- */
    /* PlcPcd = [cp0 u32][cp1 u32] + [PCD 8 bytes]
     *   cp0 = 0, cp1 = total_cp
     *   PCD: prm/flags u16=0, fc u32 (bit30 clear = 16-bit Unicode, points at
     *        fc_min in WordDocument), prm u16=0
     * Pcdt = clxt(0x02) + lcb(u32) + PlcPcd */
    rb clx = {0};
    /* single Prc terminator is optional; we go straight to the Pcdt */
    rb plc = {0};
    rb_u32(&plc, 0);                 /* cp start */
    rb_u32(&plc, total_cp);          /* cp end */
    rb_u16(&plc, 0);                 /* PCD: fNoParaLast/flags */
    rb_u32(&plc, (uint32_t)fc_min);  /* fc: bit30 clear -> 16-bit at fc_min */
    rb_u16(&plc, 0);                 /* prm */
    rb_u8(&clx, 0x02);               /* clxtPcdt */
    rb_u32(&clx, (uint32_t)plc.n);   /* lcb of the PlcPcd */
    rb_bytes(&clx, plc.b, plc.n);
    free(plc.b);

    /* fcClx @0x01A2, lcbClx @0x01A6 -> CLX at offset 0 in the table stream */
    wd.b[0x1A2] = 0x00; wd.b[0x1A3] = 0x00; wd.b[0x1A4] = 0x00; wd.b[0x1A5] = 0x00;
    wd.b[0x1A6] = (uint8_t)(clx.n);       wd.b[0x1A7] = (uint8_t)(clx.n >> 8);
    wd.b[0x1A8] = (uint8_t)(clx.n >> 16); wd.b[0x1A9] = (uint8_t)(clx.n >> 24);

    free(text.b);

    /* ---- wrap in a CFB container ---- */
    wubucfb_writer *cw = wubucfb_writer_create();
    if (!cw) { free(wd.b); free(clx.b); return -1; }
    if (wubucfb_writer_add(cw, "WordDocument", wd.b, wd.n) != 0 ||
        wubucfb_writer_add(cw, "0Table", clx.b, clx.n) != 0) {
        free(wd.b); free(clx.b); wubucfb_writer_free(cw); return -1;
    }
    uint8_t *img = NULL; size_t imglen = 0;
    int rc = wubucfb_writer_finish(cw, &img, &imglen);
    free(wd.b); free(clx.b); wubucfb_writer_free(cw);
    if (rc != 0 || !img) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) { free(img); return -1; }
    size_t wrote = fwrite(img, 1, imglen, f);
    int ok = (wrote == imglen && fclose(f) == 0) ? 0 : -1;
    free(img);
    return ok;
}
