/* png.c -- clean-room PNG decoder (see png.h). */
#define _POSIX_C_SOURCE 200809L
#include "png.h"
#include "inflate.h"

#include <stdlib.h>
#include <string.h>

#define PNG_SIG 8
static const uint8_t PNG_SIGNATURE[PNG_SIG] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

struct PngImage {
    size_t w, h;
    uint8_t *rgba;   /* w*h*4 */
};

typedef struct { uint32_t len; uint32_t type; const uint8_t *data; const uint8_t *next; } PngChunk;

static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static int next_chunk(const uint8_t *buf, size_t len, size_t pos, PngChunk *c) {
    if (pos + 12 > len) return -1;
    c->len  = rd32(buf + pos);
    c->type = rd32(buf + pos + 4);
    /* Chunk is [len][type][data*len][crc] => next = pos + 8 + len + 4. */
    if (pos + 12 + (size_t)c->len > len) return -1;   /* includes CRC */
    c->data = buf + pos + 8;
    c->next = buf + pos + 12 + (size_t)c->len;
    return 0;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

PngImage *png_decode(const uint8_t *data, size_t len) {
    if (!data || len < PNG_SIG + 12 || memcmp(data, PNG_SIGNATURE, PNG_SIG) != 0)
        return NULL;

    uint32_t width = 0, height = 0;
    int bit_depth = 0, color_type = 0, have_ihdr = 0;
    uint8_t *idat = NULL; size_t idat_len = 0;
    const uint8_t *palette = NULL; size_t palette_len = 0;
    const uint8_t *trns = NULL;    size_t trns_len = 0;

    size_t pos = PNG_SIG;
    PngChunk c;
    while (next_chunk(data, len, pos, &c) == 0) {
        if      (c.type == 0x49484452u) {  /* IHDR */
            if (c.len < 13) return NULL;
            width = rd32(c.data); height = rd32(c.data + 4);
            bit_depth = c.data[8]; color_type = c.data[9]; have_ihdr = 1;
        }
        else if (c.type == 0x504c5445u) { palette = c.data; palette_len = c.len; } /* PLTE */
        else if (c.type == 0x74524e53u) { trns = c.data; trns_len = c.len; }       /* tRNS */
        else if (c.type == 0x49444154u) {  /* IDAT */
            uint8_t *ni = realloc((void *)idat, idat_len + c.len);
            if (!ni) { free(idat); return NULL; }
            idat = ni; memcpy(idat + idat_len, c.data, c.len); idat_len += c.len;
        }
        else if (c.type == 0x49454e44u) break;   /* IEND */
        pos = (size_t)(c.next - data);
    }

    if (!have_ihdr || !idat || width == 0 || height == 0) { free(idat); return NULL; }
    if (bit_depth != 8) { free(idat); return NULL; }
    if (color_type != 0 && color_type != 2 && color_type != 3 &&
        color_type != 4 && color_type != 6) { free(idat); return NULL; }

    /* channels in the zlib stream */
    int ch = (color_type == 0) ? 1 : (color_type == 4) ? 2 :
             (color_type == 2) ? 3 : (color_type == 6) ? 4 : 1; /* palette -> 1 index */

    uint8_t *raw = NULL; size_t raw_len = 0;
    if (wubuzip_inflate(idat, idat_len, &raw, &raw_len, 1) != 0 || raw_len == 0) {
        free(idat); free(raw); return NULL;
    }
    free(idat);

    size_t stride = (size_t)width * ch;
    size_t expected = (stride + 1) * height;
    if (raw_len < expected) { free(raw); return NULL; }

    PngImage *im = calloc(1, sizeof *im);
    if (!im) { free(raw); return NULL; }
    im->w = width; im->h = height;
    im->rgba = calloc((size_t)width * height * 4, 1);
    if (!im->rgba) { free(raw); free(im); return NULL; }

    /* Reconstruction plane (one scanline of `ch` channels, raw samples). */
    uint8_t *recon = malloc(stride ? stride : 1);
    if (!recon) { free(raw); free(im->rgba); free(im); return NULL; }
    memset(recon, 0, stride);

    for (size_t y = 0; y < height; y++) {
        const uint8_t *in = raw + y * (stride + 1);
        int filter = in[0];
        const uint8_t *cur = in + 1;
        for (size_t i = 0; i < stride; i++) {
            int x = (i >= (size_t)ch) ? recon[i - ch] : 0;
            int a = x;
            int b = recon[i];
            int cc = (i >= (size_t)ch) ? recon[i - ch] : 0;
            int val = cur[i];
            switch (filter) {
                case 0: break;                                  /* None */
                case 1: val += a; break;                        /* Sub   */
                case 2: val += b; break;                        /* Up    */
                case 3: val += (a + b) / 2; break;             /* Average */
                case 4: val += paeth(a, b, cc); break;         /* Paeth */
                default: free(raw); free(recon); free(im->rgba); free(im); return NULL;
            }
            recon[i] = (uint8_t)(val & 0xFF);
        }

        /* Map the reconstructed scanline to RGBA. */
        uint8_t *out = im->rgba + y * width * 4;
        if (color_type == 0) {                 /* gray */
            for (size_t x = 0; x < width; x++) {
                uint8_t g = recon[x];
                out[x*4+0] = g; out[x*4+1] = g; out[x*4+2] = g; out[x*4+3] = 255;
            }
        } else if (color_type == 4) {          /* gray + alpha */
            for (size_t x = 0; x < width; x++) {
                uint8_t g = recon[x*2], al = recon[x*2+1];
                out[x*4+0] = g; out[x*4+1] = g; out[x*4+2] = g; out[x*4+3] = al;
            }
        } else if (color_type == 2) {          /* RGB */
            for (size_t x = 0; x < width; x++) {
                out[x*4+0] = recon[x*3]; out[x*4+1] = recon[x*3+1];
                out[x*4+2] = recon[x*3+2]; out[x*4+3] = 255;
            }
        } else if (color_type == 6) {          /* RGBA */
            memcpy(out, recon, width * 4);
        } else {                               /* palette (color_type 3) */
            for (size_t x = 0; x < width; x++) {
                size_t idx = recon[x];
                size_t po = idx * 3;
                uint8_t r = (po + 2 < palette_len) ? palette[po]   : 0;
                uint8_t g = (po + 2 < palette_len) ? palette[po+1] : 0;
                uint8_t b = (po + 2 < palette_len) ? palette[po+2] : 0;
                uint8_t a = 255;
                /* tRNS: single palette-entry alpha (index 0 usually) */
                if (trns && idx < trns_len) a = trns[idx];
                out[x*4+0] = r; out[x*4+1] = g; out[x*4+2] = b; out[x*4+3] = a;
            }
        }
    }

    free(raw); free(recon);
    return im;
}

void png_free(PngImage *im) {
    if (!im) return;
    free(im->rgba); free(im);
}

size_t png_width(const PngImage *im)  { return im ? im->w : 0; }
size_t png_height(const PngImage *im) { return im ? im->h : 0; }
const uint8_t *png_rgba(const PngImage *im) { return im ? im->rgba : NULL; }
