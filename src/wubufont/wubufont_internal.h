/* wubufont_internal.h -- PRIVATE surface for the wubufont module.
 *
 * Included by the per-concern .inc fragments (font_parse, font_name,
 * font_cmap, font_glyf_decode, font_svg, font_raster) and by the unity
 * aggregator wubufont.c. This header is NOT part of the public API
 * (wubufont.h): it carries the full (opaque-to-callers) Font struct, the
 * big-endian readers, the glyf flag-bit macros, the shared coord_cursor
 * type, and the forward declarations of the module-private helpers that the
 * fragments hand to one another. Keeping all of this in one guarded header
 * means every fragment is self-contained and the shared decode_simple_points
 * path is defined exactly once (DRY: the SVG emitter and the rasterizer reuse
 * the identical contour decoder, so the rendered outline and the SVG path can
 * never diverge).
 */
#ifndef WUBUFONT_INTERNAL_H
#define WUBUFONT_INTERNAL_H

#include "wubufont.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- big-endian readers (sfnt is big-endian on disk) ---- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int16_t  rd16s(const uint8_t *p) { return (int16_t)rd16(p); }

/* xrealloc: realloc that never returns NULL (aborts on OOM). */
static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n ? n : 1); if (!r) abort(); return r; }

/* 4-character font-table tag. */
#define TAG(a,b,c,d) ((uint32_t)((a)<<24)|((b)<<16)|((c)<<8)|(d))

/* Full (opaque to callers) Font definition. Public headers expose only
 * `typedef struct Font Font;`. The blob is referenced, not copied, unless
 * take_ownership was used (then `owned` holds the copy `data` points into). */
struct Font {
    const uint8_t *data;
    size_t size;
    /* parsed directory */
    uint16_t n_tables;
    uint32_t *tags;
    size_t   *off;
    size_t   *len;
    /* cached metrics */
    int have_head, have_maxp, have_hhea;
    uint16_t units_per_em;
    uint16_t glyph_count;
    int16_t  ascent, descent;
    /* loca cache (glyph offsets), length glyph_count+1, in bytes */
    const uint8_t *loca;     /* points into data if present */
    int loca_is_long;        /* 1 => 4-byte offsets, 0 => 2-byte */
    const uint8_t *glyf;
    uint8_t *owned;          /* if non-NULL, a heap copy of `data` we own */
};

/* ---- glyf coordinate-decoder flag bits (shared by font_glyf_decode) ---- */
/* Stored per point: on-curve bit in bit0, x/y signedness hints in the high
 * bits of `on[]`, so a single compact array carries everything the decoder
 * needs in one pass. */
#define ON_BIT      0x01
#define XSHORT_POS  0x02
#define XSHORT_NEG  0x10
#define YSHORT_POS  0x04
#define YSHORT_NEG  0x20

/* Cursor over the variable-length TrueType coordinate byte stream. */
typedef struct { const uint8_t *data; size_t cp; } coord_cursor;

/* ---- forward declarations of module-private helpers shared across .inc ----
 * Every one of these is `static`, so they are invisible outside this single
 * translation unit (the unity build) and cannot leak into other modules. */
static size_t  glyph_offset(const Font *f, uint16_t gi);
static size_t  glyph_next(const Font *f, uint16_t gi);
static uint16_t decode_simple_points(const uint8_t *g, uint16_t n,
                                     int16_t *X, int16_t *Y, uint8_t *on);
static char   *font_glyph_decode_simple(const Font *f, const uint8_t *g, uint16_t n);
static int16_t coord_next(coord_cursor *c, uint8_t flag, int is_y);
static void    decode_flags_and_coords(const uint8_t *flags, size_t flaglen,
                                       coord_cursor *cc, int is_y,
                                       int16_t *out, uint8_t *on, uint16_t total);
static size_t  flatten_contour(const int16_t *X, const int16_t *Y, const uint8_t *on,
                               size_t n, double s, int16_t *out);
static void    scanline_fill(const int16_t *verts, const size_t *cnt, size_t nc,
                             int w, int h, uint8_t *bits);
static size_t  font_glyph_contours_idx(const Font *f, uint16_t gi,
                                       double scale, FontContour *out, size_t max_out);

#endif /* WUBUFONT_INTERNAL_H */
