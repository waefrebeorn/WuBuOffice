/* image.c -- WuBuOCR grayscale page buffer + clean-room Netpbm decoder. */
#include "image.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

struct OcrImage {
    size_t w, h;
    uint8_t *px;   /* w*h bytes, row-major, 0=black..255=white */
};

OcrImage *ocr_image_create(size_t w, size_t h) {
    if (w == 0 || h == 0) return NULL;
    if (w > SIZE_MAX / h) return NULL;          /* area overflow */
    OcrImage *im = malloc(sizeof *im);
    if (!im) return NULL;
    im->w = w; im->h = h;
    im->px = malloc(w * h);
    if (!im->px) { free(im); return NULL; }
    memset(im->px, 255, w * h);
    return im;
}

void ocr_image_free(OcrImage *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

size_t ocr_image_width(const OcrImage *im)  { return im ? im->w : 0; }
size_t ocr_image_height(const OcrImage *im) { return im ? im->h : 0; }

uint8_t ocr_image_get(const OcrImage *im, size_t x, size_t y) {
    if (!im || x >= im->w || y >= im->h) return 255;
    return im->px[y * im->w + x];
}

void ocr_image_set(OcrImage *im, size_t x, size_t y, uint8_t v) {
    if (!im || x >= im->w || y >= im->h) return;
    im->px[y * im->w + x] = v;
}

const uint8_t *ocr_image_pixels(const OcrImage *im) { return im ? im->px : NULL; }

/* ---------- Netpbm parsing helpers ---------- */
typedef struct { const uint8_t *p, *end; } Scan;

/* Skip whitespace and '#'-to-EOL comments (Netpbm allows comments in the
 * header up to and including the maxval token). */
static void pnm_skip_ws(Scan *s) {
    for (;;) {
        while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' ||
                                 *s->p == '\n' || *s->p == '\r' ||
                                 *s->p == '\f' || *s->p == '\v')) s->p++;
        if (s->p < s->end && *s->p == '#') {
            while (s->p < s->end && *s->p != '\n') s->p++;
            continue;
        }
        break;
    }
}

/* Parse a non-negative decimal integer header token. Returns 0 on success,
 * -1 if no digit is present. */
static int pnm_uint(Scan *s, unsigned long *out) {
    pnm_skip_ws(s);
    if (s->p >= s->end || *s->p < '0' || *s->p > '9') return -1;
    unsigned long v = 0;
    while (s->p < s->end && *s->p >= '0' && *s->p <= '9') {
        v = v * 10u + (unsigned long)(*s->p - '0');
        s->p++;
    }
    *out = v;
    return 0;
}

/* Netpbm: PBM 1=black, 0=white -> our 0=black,255=white (inverted).
 * PGM/PPM: sample scaled from [0,maxval] to [0,255], white=high. */
static uint8_t scale_sample(unsigned long v, unsigned long maxval) {
    if (maxval == 0) return 255;
    if (v > maxval) v = maxval;
    return (uint8_t)((v * 255u) / maxval);
}

OcrImage *ocr_image_from_netpbm(const uint8_t *data, size_t len) {
    if (!data || len < 2 || data[0] != 'P') return NULL;
    int magic = data[1] - '0';
    if (magic < 1 || magic > 6) return NULL;

    Scan s = { data + 2, data + len };
    unsigned long w = 0, h = 0, maxval = 255;
    if (pnm_uint(&s, &w) != 0 || pnm_uint(&s, &h) != 0) return NULL;
    if (w == 0 || h == 0) return NULL;
    int is_bitmap = (magic == 1 || magic == 4);
    if (!is_bitmap) {
        if (pnm_uint(&s, &maxval) != 0 || maxval == 0 || maxval > 65535) return NULL;
    }

    OcrImage *im = ocr_image_create((size_t)w, (size_t)h);
    if (!im) return NULL;
    size_t n = (size_t)w * (size_t)h;

    if (magic <= 3) {
        /* ASCII: P1 bitmap, P2 gray, P3 rgb */
        for (size_t i = 0; i < n; i++) {
            if (magic == 1) {
                pnm_skip_ws(&s);
                if (s.p >= s.end || (*s.p != '0' && *s.p != '1')) { ocr_image_free(im); return NULL; }
                im->px[i] = (*s.p == '1') ? 0 : 255;   /* 1=black */
                s.p++;
            } else if (magic == 2) {
                unsigned long v;
                if (pnm_uint(&s, &v) != 0) { ocr_image_free(im); return NULL; }
                im->px[i] = scale_sample(v, maxval);
            } else { /* magic == 3, RGB */
                unsigned long r, g, b;
                if (pnm_uint(&s, &r) || pnm_uint(&s, &g) || pnm_uint(&s, &b)) {
                    ocr_image_free(im); return NULL;
                }
                /* Rec.601 luma on scaled channels */
                unsigned long lr = scale_sample(r, maxval);
                unsigned long lg = scale_sample(g, maxval);
                unsigned long lb = scale_sample(b, maxval);
                im->px[i] = (uint8_t)((299u * lr + 587u * lg + 114u * lb) / 1000u);
            }
        }
        return im;
    }

    /* Binary P4/P5/P6: exactly one whitespace byte separates header and data. */
    if (s.p < s.end) s.p++;

    if (magic == 4) { /* PBM: packed bits, MSB first, rows byte-padded */
        size_t row_bytes = ((size_t)w + 7) / 8;
        if ((size_t)(s.end - s.p) < row_bytes * (size_t)h) { ocr_image_free(im); return NULL; }
        for (size_t y = 0; y < (size_t)h; y++) {
            const uint8_t *row = s.p + y * row_bytes;
            for (size_t x = 0; x < (size_t)w; x++) {
                int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
                im->px[y * (size_t)w + x] = bit ? 0 : 255;   /* 1=black */
            }
        }
        return im;
    }

    if (magic == 5) { /* PGM binary */
        size_t bytes_per = (maxval > 255) ? 2 : 1;
        if ((size_t)(s.end - s.p) < n * bytes_per) { ocr_image_free(im); return NULL; }
        for (size_t i = 0; i < n; i++) {
            unsigned long v = bytes_per == 2
                ? ((unsigned long)s.p[i * 2] << 8) | s.p[i * 2 + 1]
                : s.p[i];
            im->px[i] = scale_sample(v, maxval);
        }
        return im;
    }

    /* magic == 6: PPM binary RGB */
    {
        size_t bytes_per = (maxval > 255) ? 2 : 1;
        size_t stride = 3 * bytes_per;
        if ((size_t)(s.end - s.p) < n * stride) { ocr_image_free(im); return NULL; }
        for (size_t i = 0; i < n; i++) {
            const uint8_t *pp = s.p + i * stride;
            unsigned long r, g, b;
            if (bytes_per == 2) {
                r = ((unsigned long)pp[0] << 8) | pp[1];
                g = ((unsigned long)pp[2] << 8) | pp[3];
                b = ((unsigned long)pp[4] << 8) | pp[5];
            } else { r = pp[0]; g = pp[1]; b = pp[2]; }
            unsigned long lr = scale_sample(r, maxval);
            unsigned long lg = scale_sample(g, maxval);
            unsigned long lb = scale_sample(b, maxval);
            im->px[i] = (uint8_t)((299u * lr + 587u * lg + 114u * lb) / 1000u);
        }
        return im;
    }
}

int ocr_image_to_pgm(const OcrImage *im, uint8_t **out, size_t *out_len) {
    if (!im || !out || !out_len) return -1;
    char hdr[64];
    int hn = snprintf(hdr, sizeof hdr, "P5\n%zu %zu\n255\n", im->w, im->h);
    if (hn < 0) return -1;
    size_t n = im->w * im->h;
    uint8_t *buf = malloc((size_t)hn + n);
    if (!buf) return -1;
    memcpy(buf, hdr, (size_t)hn);
    memcpy(buf + hn, im->px, n);
    *out = buf;
    *out_len = (size_t)hn + n;
    return 0;
}
