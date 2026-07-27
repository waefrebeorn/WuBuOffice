/* wubupng.c -- single correct PNG encoder (see wubupng.h). Clean C11.
 * Reuses zlib's crc32 + compress (system lib, not bundled third-party). */
#include "wubupng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void put_u32(unsigned char *o, unsigned long v){
    o[0] = (unsigned char)((v >> 24) & 0xff);
    o[1] = (unsigned char)((v >> 16) & 0xff);
    o[2] = (unsigned char)((v >> 8) & 0xff);
    o[3] = (unsigned char)(v & 0xff);
}

/* Append one chunk to a growing buffer: [4 len][4 type][data][4 crc] */
static int emit_chunk(unsigned char **buf, size_t *len, size_t *cap,
                      const char *type, const unsigned char *data, unsigned long dlen){
    size_t need = *len + 4 + 4 + (size_t)dlen + 4;
    if (need > *cap){ size_t nc = *cap ? *cap : 1024; while (nc < need) nc *= 2;
        unsigned char *nb = realloc(*buf, nc); if (!nb) return -1; *buf = nb; *cap = nc; }
    put_u32(*buf + *len, dlen);                 *len += 4;
    memcpy(*buf + *len, type, 4);               *len += 4;
    if (dlen && data){ memcpy(*buf + *len, data, dlen); *len += dlen; }
    /* CRC over type+data, incremental (safe for any dlen, no fixed buffer) */
    unsigned long c = crc32(crc32(0, (const Bytef*)type, 4),
                            data ? data : (const Bytef*)"", dlen);
    put_u32(*buf + *len, c);                    *len += 4;
    return 0;
}

int wubupng_encode(int fmt, const void *pixels, uint32_t W, uint32_t H,
                   uint8_t **out, size_t *out_len){
    if (!pixels || !out || !out_len) return -1;
    int depth, color_type, bpp;
    if (fmt == WUBUPNG_RGBA){ depth = 8; color_type = 6; bpp = 4; }
    else if (fmt == WUBUPNG_GRAY8){ depth = 8; color_type = 0; bpp = 1; }
    else return -1;

    unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    unsigned char ihdr[13];
    put_u32(ihdr + 0, (unsigned long)W);
    put_u32(ihdr + 4, (unsigned long)H);
    ihdr[8]  = (unsigned char)depth;
    ihdr[9]  = (unsigned char)color_type;
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;

    /* raw scanlines with a leading filter byte (0) per row */
    size_t raw = (size_t)H * (1 + (size_t)W * bpp);
    unsigned char *rawp = malloc(raw ? raw : 1);
    if (!rawp) return -1;
    for (uint32_t y = 0; y < H; y++){
        size_t dst = (size_t)y * (1 + (size_t)W * bpp);
        rawp[dst] = 0;
        memcpy(rawp + dst + 1, (const unsigned char*)pixels + (size_t)y * W * bpp,
               (size_t)W * bpp);
    }

    uLong cl = compressBound(raw);
    unsigned char *cmp = malloc(cl ? cl : 1);
    if (!cmp){ free(rawp); return -1; }
    if (compress(cmp, &cl, rawp, raw) != Z_OK){ free(rawp); free(cmp); return -1; }

    unsigned char *buf = NULL; size_t len = 0, cap = 0;

    /* 8-byte PNG signature (NOT a chunk) */
    {
        size_t need = 8;
        if (need > cap){ size_t nc = 1024; while (nc < need) nc *= 2;
            unsigned char *nb = realloc(buf, nc); if (!nb){ free(rawp); free(cmp); return -1; }
            buf = nb; cap = nc; }
        memcpy(buf + len, sig, 8); len += 8;
    }

    if (emit_chunk(&buf, &len, &cap, "IHDR", ihdr, 13)) { free(rawp); free(cmp); free(buf); return -1; }
    if (emit_chunk(&buf, &len, &cap, "IDAT", cmp, (unsigned long)cl)) { free(rawp); free(cmp); free(buf); return -1; }
    if (emit_chunk(&buf, &len, &cap, "IEND", NULL, 0)) { free(rawp); free(cmp); free(buf); return -1; }

    free(rawp); free(cmp);
    *out = buf; *out_len = len;
    return 0;
}

int wubupng_write_file(const char *path, int fmt, const void *pixels,
                       uint32_t W, uint32_t H){
    uint8_t *buf; size_t len;
    if (wubupng_encode(fmt, pixels, W, H, &buf, &len) != 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f){ free(buf); return -1; }
    size_t wrote = fwrite(buf, 1, len, f);
    int ok = (wrote == len) && (fclose(f) == 0);
    free(buf);
    return ok ? 0 : -1;
}
