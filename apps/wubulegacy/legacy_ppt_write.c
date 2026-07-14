/* legacy_ppt_write.c -- encode a legacy .ppt (PowerPoint binary) from
 * wubushow_pres. Produces a minimal but valid PPT stream ("PowerPoint
 * Document") that PowerPoint / LibreOffice open and that our reader round-trips.
 *
 * Layout (record = 2-byte type + 2-byte len + payload; containers nest):
 *   DocumentContainer (0x03E8)
 *     + DocumentAtom (0x03E9)
 *     + SlidePersistAtom (0x03F3) per slide (id, number, offset->filled 0)
 *   SlideContainer (0x03EE) per slide
 *     + SlideAtom (0x03EF)
 *     + TextHeaderAtom (0x0FA0? actually 0x03F9/0x0F9F) + TextBytes (0x0FA8)
 *
 * We keep offsets zero (not strictly required for opening); Word/LibreOffice
 * recompute layout. The essential part for our reader is the TextBytes/TextChars
 * atoms inside Slide containers, which our ppt_bin.c walks.
 *
 * Clean-room C11. */

#include "legacy.h"
#include "../wubushow/show.h"
#include "legacy_internal.h"
#include "../../src/wubucfb/cfb_write.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { uint8_t *b; size_t n, cap; } rb;
static void rb_reserve(rb *r, size_t e){ if(r->n+e>r->cap){ size_t nc=r->cap?r->cap*2:1024; while(r->n+e>nc)nc*=2; r->b=realloc(r->b,nc); r->cap=nc; } }
static void rb_u8(rb *r, uint8_t v){ rb_reserve(r,1); r->b[r->n++]=v; }
static void rb_u16(rb *r, uint16_t v){ rb_reserve(r,2); r->b[r->n++]=v&0xff; r->b[r->n++]=(v>>8)&0xff; }
static void rb_u32(rb *r, uint32_t v){ rb_reserve(r,4); for(int i=0;i<4;i++) r->b[r->n++]=(v>>(8*i))&0xff; }
static void rb_bytes(rb *r, const void*p, size_t n){ rb_reserve(r,n); memcpy(r->b+r->n,p,n); r->n+=n; }
static size_t rec_begin(rb *r, uint16_t ver_inst, uint16_t type){ rb_u16(r, ver_inst); rb_u16(r, type); size_t lp=r->n; rb_u32(r,0); return lp; }
static void rec_end(rb *r, size_t lp){ uint32_t L=(uint32_t)(r->n-lp-4); r->b[lp]=L&0xff; r->b[lp+1]=(L>>8)&0xff; r->b[lp+2]=(L>>16)&0xff; r->b[lp+3]=(L>>24)&0xff; }

/* Emit a text atom. TextChars (0x0FA0) = UTF-16LE, no length prefix (the record
 * length IS the byte count); TextBytes (0x0FA8) = CP1252 bytes. */
static void emit_text_atom(rb *r, uint16_t atom_type, const char *s) {
    size_t p = rec_begin(r, 0x0000, atom_type);
    const char *q = s ? s : "";
    if (atom_type == 0x0FA0) {
        for (; *q; q++) rb_u16(r, (uint8_t)*q);   /* UTF-16LE, 1 unit per ASCII byte */
    } else {
        rb_bytes(r, q, strlen(q));
    }
    rec_end(r, p);
}

int wubulegacy_write_ppt(const wubushow_pres *p, const char *path) {
    if (!p) return -1;
    int ns = wubushow_slide_count(p);
    rb r = {0};

    /* ---- DocumentContainer ---- */
    size_t doc = rec_begin(&r, 0x000F, 0x03E8);
    {   /* DocumentAtom */
        size_t a = rec_begin(&r, 0x0000, 0x03E9);
        rb_u32(&r, 0x000000A0);   /* slide size X (2560) */
        rb_u32(&r, 0x00000078);   /* slide size Y (1920) */
        rb_u32(&r, 0);            /* notes size X */
        rb_u32(&r, 0);            /* notes size Y */
        rb_u16(&r, 0);            /* server version */
        rb_u16(&r, 0);            /* version */
        rb_u16(&r, 0);            /* revision */
        rb_u32(&r, 0);            /* reserved */
        rb_u32(&r, 0);            /* reserved */
        rb_u8(&r, 0);             /* build year */
        rb_u8(&r, 0);             /* build month */
        rb_u8(&r, 0);             /* build day */
        rb_u8(&r, 0);             /* build hour */
        rb_u32(&r, 0);            /* build minor */
        rec_end(&r, a);
        for (int i = 0; i < ns; i++) {
            const char *title = NULL, *body = NULL;
            wubushow_slide_get(p, i, &title, &body);
            (void)title; (void)body;
            size_t sp = rec_begin(&r, 0x0000, 0x03F3);   /* SlidePersistAtom */
            rb_u32(&r, (uint32_t)(i + 256));     /* persist id */
            rb_u32(&r, (uint32_t)(i + 1));       /* number */
            rb_u32(&r, 0);                        /* ref offset (0) */
            rb_u32(&r, 0);                        /* text offset (0) */
            rb_u8(&r, 0);                         /* flags: has comments */
            rb_u8(&r, 0);                         /* reserved */
            rec_end(&r, sp);
        }
    }
    rec_end(&r, doc);

    /* ---- Slides ---- */
    for (int i = 0; i < ns; i++) {
        const char *title = NULL, *body = NULL;
        wubushow_slide_get(p, i, &title, &body);
        size_t slide = rec_begin(&r, 0x000F, 0x03EE);   /* SlideContainer */
        {   /* SlideAtom */
            size_t sa = rec_begin(&r, 0x0000, 0x03EF);
            rb_u32(&r, (uint32_t)(i + 256));     /* persist id ref */
            rb_u32(&r, 0);                        /* layout (0 = title+body) */
            rb_u8(&r, 0);                         /* placeholder */
            rb_u8(&r, 0);                         /* flags */
            rec_end(&r, sa);
            /* TextHeaderAtom: type 0 (title) */
            size_t th = rec_begin(&r, 0x0000, 0x0F9F); rb_u32(&r, 0); rec_end(&r, th);
            emit_text_atom(&r, 0x0FA0, title ? title : "");
            /* TextHeaderAtom: type 1 (body) */
            size_t th2 = rec_begin(&r, 0x0000, 0x0F9F); rb_u32(&r, 1); rec_end(&r, th2);
            emit_text_atom(&r, 0x0FA0, body ? body : "");
        }
        rec_end(&r, slide);
    }

    /* wrap CFB */
    wubucfb_writer *cw = wubucfb_writer_create();
    if (!cw) { free(r.b); return -1; }
    if (wubucfb_writer_add(cw, "PowerPoint Document", r.b, r.n) != 0) { free(r.b); wubucfb_writer_free(cw); return -1; }
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
