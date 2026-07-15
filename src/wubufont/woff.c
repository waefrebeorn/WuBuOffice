/* woff.c -- clean-room WOFF 1.0 reader + writer. Native C11, no deps.
 * Reuses wubuzip (raw DEFLATE) for the zlib (RFC 1950) table compression. */
#include "woff.h"
#include "wubufont.h"
#include "inflate.h"
#include "deflate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n ? n : 1); if (!r) abort(); return r; }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0]<<8)|p[1]); }

/* Adler-32 (zlib). MOD is 65521. */
uint32_t woff_adler32(const uint8_t *data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

/* zlib-compress (RFC 1950): 0x78 0x9C header + raw deflate + adler32 LE.
 * Returns malloc'd blob (caller frees) and *out_len. NULL on failure. */
static uint8_t *zlib_compress(const uint8_t *in, size_t in_len, size_t *out_len) {
    uint8_t *raw = NULL; size_t raw_len = 0;
    if (wubuzip_deflate(in, in_len, &raw, &raw_len) != 0) return NULL;
    size_t total = 2 + raw_len + 4;
    uint8_t *z = xrealloc(NULL, total);
    z[0] = 0x78; z[1] = 0x9C;
    memcpy(z + 2, raw, raw_len);
    uint32_t ad = woff_adler32(in, in_len);
    z[2 + raw_len]     = (uint8_t)(ad & 0xFF);
    z[2 + raw_len + 1] = (uint8_t)((ad >> 8) & 0xFF);
    z[2 + raw_len + 2] = (uint8_t)((ad >> 16) & 0xFF);
    z[2 + raw_len + 3] = (uint8_t)((ad >> 24) & 0xFF);
    free(raw);
    *out_len = total;
    return z;
}

/* zlib-decompress (RFC 1950): strip 2-byte header, inflate with zlib header
 * awareness, then verify the trailing Adler-32 against the ORIGINAL data.
 * Returns malloc'd blob (caller frees) and *out_len. NULL on failure. */
static uint8_t *zlib_decompress(const uint8_t *in, size_t in_len, size_t *out_len, size_t orig_len) {
    if (in_len < 6) return NULL;  /* header(2) + at least 4 trailer */
    uint8_t *raw = NULL; size_t raw_len = 0;
    /* wubuzip_inflate with_zlib_header=1 itself skips the 2-byte zlib header.
     * We give it the full stream MINUS our 4-byte Adler trailer, and verify
     * that trailer ourselves. */
    if (wubuzip_inflate(in, in_len - 4, &raw, &raw_len, 1) != 0) { free(raw); return NULL; }
    /* verify adler-32 of the reconstructed original */
    uint32_t got = ((uint32_t)in[in_len-4]) | ((uint32_t)in[in_len-3]<<8)
                 | ((uint32_t)in[in_len-2]<<16) | ((uint32_t)in[in_len-1]<<24);
    if (got != woff_adler32(raw, raw_len)) { free(raw); return NULL; }
    (void)orig_len;
    *out_len = raw_len;
    return raw;
}

/* ---- WOFF reading ---- */
Font *woff_open(const uint8_t *data, size_t size) {
    if (!data || size < 44) return NULL;
    if (rd32(data) != 0x774F4646u) return NULL;   /* 'wOFF' */
    uint16_t numTables = rd16(data + 12);
    uint32_t totalSfntSize = rd32(data + 16);
    (void)totalSfntSize;

    /* first pass: gather directory */
    typedef struct { uint32_t tag; uint32_t off; uint32_t comp; uint32_t orig; uint32_t chk; } WDir;
    WDir *dir = xrealloc(NULL, (size_t)numTables * sizeof *dir);
    const uint8_t *dp = data + 44;
    for (uint16_t i = 0; i < numTables; i++, dp += 20) {
        dir[i].tag  = rd32(dp);
        dir[i].off  = rd32(dp + 4);
        dir[i].comp = rd32(dp + 8);
        dir[i].orig = rd32(dp + 12);
        dir[i].chk  = rd32(dp + 16);
    }

    /* build the sfnt: 12-byte offset table + numTables*16 directory + tables */
    size_t sfnt_cap = 12 + (size_t)numTables * 16;
    for (uint16_t i = 0; i < numTables; i++) sfnt_cap += dir[i].orig + 3; /* +3 pad to 4 */
    uint8_t *sfnt = xrealloc(NULL, sfnt_cap);
    /* offset table */
    uint32_t flavor = rd32(data + 4);
    sfnt[0] = (uint8_t)(flavor >> 24); sfnt[1] = (uint8_t)(flavor >> 16);
    sfnt[2] = (uint8_t)(flavor >> 8);  sfnt[3] = (uint8_t)flavor;
    sfnt[4] = (uint8_t)(numTables >> 8); sfnt[5] = (uint8_t)numTables;
    sfnt[6] = data[6]; sfnt[7] = data[7];            /* searchRange (copy) */
    sfnt[8] = data[8]; sfnt[9] = data[9];            /* entrySelector */
    sfnt[10] = data[10]; sfnt[11] = data[11];        /* rangeShift */
    size_t dir_off = 12;
    size_t body_off = 12 + (size_t)numTables * 16;
    for (uint16_t i = 0; i < numTables; i++) {
        /* directory entry: tag(4) checksum(4) offset(4) length(4) */
        uint8_t *e = sfnt + dir_off + (size_t)i * 16;
        e[0] = (uint8_t)(dir[i].tag >> 24); e[1] = (uint8_t)(dir[i].tag >> 16);
        e[2] = (uint8_t)(dir[i].tag >> 8);  e[3] = (uint8_t)dir[i].tag;
        /* checksum */
        e[4] = (uint8_t)(dir[i].chk >> 24); e[5] = (uint8_t)(dir[i].chk >> 16);
        e[6] = (uint8_t)(dir[i].chk >> 8);  e[7] = (uint8_t)dir[i].chk;
        /* table offset = current body position (we write in order) */
        uint32_t toff = (uint32_t)(body_off);
        e[8] = (uint8_t)(toff >> 24); e[9] = (uint8_t)(toff >> 16);
        e[10]= (uint8_t)(toff >> 8);  e[11]= (uint8_t)toff;
        /* length (original, uncompressed) */
        e[12]= (uint8_t)(dir[i].orig >> 24); e[13]= (uint8_t)(dir[i].orig >> 16);
        e[14]= (uint8_t)(dir[i].orig >> 8);  e[15]= (uint8_t)dir[i].orig;

        /* decompress table */
        const uint8_t *src = data + dir[i].off;
        size_t src_len = dir[i].comp;
        uint8_t *tbl; size_t tbl_len;
        if (dir[i].comp == dir[i].orig) {
            /* stored uncompressed */
            tbl = xrealloc(NULL, dir[i].orig ? dir[i].orig : 1);
            memcpy(tbl, src, dir[i].orig);
            tbl_len = dir[i].orig;
        } else {
            tbl = zlib_decompress(src, src_len, &tbl_len, dir[i].orig);
            if (!tbl || tbl_len != dir[i].orig) { free(tbl); free(sfnt); free(dir); return NULL; }
        }
        memcpy(sfnt + body_off, tbl, dir[i].orig);
        free(tbl);
        body_off += dir[i].orig;
        /* pad to 4-byte boundary */
        while (body_off & 3) sfnt[body_off++] = 0;
    }
    free(dir);

    Font *f = font_open_owned(sfnt, body_off, 1);   /* copies sfnt internally */
    free(sfnt);                                      /* our build buffer; the Font owns its own copy */
    return f;
}

/* ---- WOFF writing (also the round-trip oracle for woff_open) ---- */
uint8_t *sfnt_to_woff(const uint8_t *sfnt, size_t sfnt_len, size_t *out_len) {
    if (!sfnt || sfnt_len < 12) return NULL;
    uint32_t sig = rd32(sfnt);
    if (sig != 0x00010000u && sig != 0x74727565u /*'true'*/ && sig != 0x4F54544Fu /*'OTTO'*/)
        return NULL;
    uint16_t numTables = rd16(sfnt + 4);

    /* gather sfnt table directory */
    typedef struct { uint32_t tag; uint32_t off; uint32_t len; uint32_t chk; } SDir;
    SDir *sd = xrealloc(NULL, (size_t)numTables * sizeof *sd);
    for (uint16_t i = 0; i < numTables; i++) {
        const uint8_t *e = sfnt + 12 + (size_t)i * 16;
        sd[i].tag = rd32(e);
        sd[i].off = rd32(e + 8);
        sd[i].len = rd32(e + 12);
        sd[i].chk = rd32(e + 16);
    }

    /* First, compress every table to learn compressed sizes. */
    uint8_t **comp = xrealloc(NULL, (size_t)numTables * sizeof *comp);
    size_t   *clen = xrealloc(NULL, (size_t)numTables * sizeof *clen);
    for (uint16_t i = 0; i < numTables; i++) {
        const uint8_t *tdata = sfnt + sd[i].off;
        /* if table is tiny, storing uncompressed is smaller than zlib header */
        uint8_t *z = zlib_compress(tdata, sd[i].len, &clen[i]);
        if (!z) { /* fall back to stored */
            z = xrealloc(NULL, sd[i].len ? sd[i].len : 1);
            memcpy(z, tdata, sd[i].len);
            clen[i] = sd[i].len;
        }
        /* WOFF: if compressed >= original, store uncompressed */
        if (clen[i] >= sd[i].len) {
            free(z);
            z = xrealloc(NULL, sd[i].len ? sd[i].len : 1);
            memcpy(z, tdata, sd[i].len);
            clen[i] = sd[i].len;
        }
        comp[i] = z;
    }

    /* compute total size: header(44) + dir(numTables*20) + sum(comp padded 4) */
    size_t total = 44 + (size_t)numTables * 20;
    for (uint16_t i = 0; i < numTables; i++) {
        size_t p = clen[i];
        while (p & 3) p++;
        total += p;
    }

    uint8_t *out = xrealloc(NULL, total);
    /* compute totalSfntSize = 12 + numTables*16 + sum(orig padded to 4) */
    size_t totalSfnt = 12 + (size_t)numTables * 16;
    for (uint16_t i = 0; i < numTables; i++) {
        size_t pp = sd[i].len;
        while (pp & 3) pp++;
        totalSfnt += pp;
    }
    /* WOFF 1.0 header is 44 bytes with this exact layout (big-endian):
     *  0 signature(4)='wOFF'  4 flavor(4)  8 length(4)  12 numTables(2)
     * 14 reserved(2)  16 totalSfntSize(4)  20 majorVersion(2) 22 minorVersion(2)
     * 24 metaOffset(4) 28 metaLength(4) 32 metaOrigLength(4)
     * 36 privOffset(4) 40 privLength(4). NOTE: WOFF does NOT carry the sfnt
     * offset-table's searchRange/entrySelector/rangeShift. */
    size_t p = 0;
    uint8_t *W = out;
    #define PUT32(v) do { W[p++]=(uint8_t)((uint32_t)(v)>>24); W[p++]=(uint8_t)((uint32_t)(v)>>16); W[p++]=(uint8_t)((uint32_t)(v)>>8); W[p++]=(uint8_t)(v); } while(0)
    #define PUT16(v) do { W[p++]=(uint8_t)((uint16_t)(v)>>8); W[p++]=(uint8_t)(v); } while(0)
    PUT32(0x774F4646u);        /* 'wOFF'          @0  */
    PUT32(sig);                /* flavor          @4  */
    PUT32((uint32_t)total);    /* length          @8  */
    PUT16(numTables);          /* numTables       @12 */
    PUT16(0);                  /* reserved        @14 */
    PUT32((uint32_t)totalSfnt);/* totalSfntSize   @16 */
    PUT16(0);                  /* majorVersion    @20 */
    PUT16(0);                  /* minorVersion    @22 */
    PUT32(0);                  /* metaOffset      @24 */
    PUT32(0);                  /* metaLength      @28 */
    PUT32(0);                  /* metaOrigLength  @32 */
    PUT32(0);                  /* privOffset      @36 */
    PUT32(0);                  /* privLength      @40 */
    /* table directory (20 bytes each): tag(4) offset(4) compLen(4) origLen(4) origChecksum(4) */
    size_t data_off = 44 + (size_t)numTables * 20;
    size_t cur = data_off;
    for (uint16_t i = 0; i < numTables; i++) {
        PUT32(sd[i].tag);
        PUT32((uint32_t)cur);
        PUT32((uint32_t)clen[i]);
        PUT32(sd[i].len);
        PUT32(sd[i].chk);
        cur += clen[i];
        while (cur & 3) cur++;  /* offsets align to 4 */
    }
    /* table data */
    for (uint16_t i = 0; i < numTables; i++) {
        memcpy(W + data_off, comp[i], clen[i]);
        data_off += clen[i];
        while (data_off & 3) W[data_off++] = 0;
    }
    #undef PUT32
    #undef PUT16

    for (uint16_t i = 0; i < numTables; i++) free(comp[i]);
    free(comp); free(clen); free(sd);
    *out_len = total;
    return out;
}
