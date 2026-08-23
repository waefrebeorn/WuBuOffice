#include "wubufont_internal.h"

static int16_t coord_next(coord_cursor *c, uint8_t flag, int is_y) {
    uint8_t shp = is_y ? YSHORT_POS : XSHORT_POS;
    uint8_t shn = is_y ? YSHORT_NEG : XSHORT_NEG;
    int16_t v;
    if (flag & shp) {
        /* Short coordinate: the stored byte is an UNSIGNED MAGNITUDE; the
         * sign bit in the flag decides the direction (Apple TT spec,
         * 'glyf' simple-glyph flags). The old code read the byte as SIGNED
         * and then negated it when the sign bit was clear -- which produced
         * garbage like +66 where the true delta was -190. That desynced
         * every subsequent coordinate in curved glyphs ('e','r','d'), which
         * rendered as corrupted "invented font" shapes. */
        uint8_t m = c->data[c->cp];
        v = (int16_t)((flag & shn) ? m : -(int16_t)m);
        c->cp++;
    } else if (flag & shn) {
        v = 0;                                        /* same as previous */
    } else {
        v = (int16_t)((uint16_t)c->data[c->cp] << 8 | c->data[c->cp + 1]); /* 2-byte */
        c->cp += 2;
    }
    return v;
}
static void decode_flags_and_coords(const uint8_t *flags, size_t flaglen,
                                  coord_cursor *cc, int is_y,
                                  int16_t *out, uint8_t *on, uint16_t total) {
    uint8_t active = 0;
    uint8_t repleft = 0;
    size_t fi = 0;
    int16_t cur = 0;
    for (uint16_t pi = 0; pi < total; pi++) {
        if (repleft == 0) {
            active = flags[fi++];
            if (active & 0x08) repleft = flags[fi++];   /* repeat count */
        } else {
            repleft--;
        }
        on[pi] = (uint8_t)(active & ON_BIT);
        cur += coord_next(cc, active, is_y);
        out[pi] = cur;
    }
    (void)flaglen;
}
/* Compute the true number of points a simple glyph carries (the value the
 * end-of-contours table implies plus one), WITHOUT decoding coordinates.
 * Used by callers that must size heap buffers to the real count. Caps at the
 * same 1024 limit decode_simple_points uses, so the two can never disagree. */
static uint16_t font_glyph_point_count(const uint8_t *g, uint16_t n) {
    if (n == 0) return 0;
    uint16_t total = (uint16_t)(rd16(g + 10 + (size_t)(n - 1) * 2) + 1);
    if (total > 1024) total = 1024;
    return total;
}
static uint16_t decode_simple_points(const uint8_t *g, uint16_t n,
                                     int16_t *X, int16_t *Y, uint8_t *on) {
    uint16_t instrlen = rd16(g + 10 + (size_t)n * 2);
    /* Layout after the header (ncontours@0, bbox@2..10):
     *   endPtsOfContours[n] @10, instructionLength(2) @10+n*2,
     *   instructions[instrlen], THEN flags, THEN coordinates.
     * The flag byte stream is *variable length* (REPEAT 0x08 flags embed a
     * count byte), so the coordinate stream starts after the expanded flag
     * bytes — NOT after `instrlen`. We expand flags first to find that
     * offset, exactly as the verified reference decoder does. */
    const uint8_t *flags = g + 10 + (size_t)n * 2 + 2 + instrlen;
    uint16_t total = (n == 0) ? 0 : (uint16_t)(rd16(g + 10 + (size_t)(n - 1) * 2) + 1);
    if (total > 1024) total = 1024;
    /* Find where the coordinate stream begins by walking the FLAG BYTE
     * stream (NOT points): each distinct flag is 1 byte, plus a count
     * byte when the REPEAT (0x08) bit is set. Repeated points do NOT
     * consume extra flag bytes, so counting once-per-point (the previous
     * buggy version) over-counts and shifts the coords off by exactly the
     * number of repeat-count bytes. */
    size_t fi = 0;
    for (uint16_t pi = 0; pi < total; ) {
        uint8_t f = flags[fi++];
        if (f & 0x08) {
            uint8_t rep = flags[fi++];   /* repeat count byte */
            pi += (uint16_t)rep + 1;   /* this flag + `rep` more points */
        } else {
            pi += 1;
        }
    }
    const uint8_t *xcoords = flags + fi;
    /* X pass, then Y pass. Each pass replays the SAME flag stream, so the
     * Y coordinates begin precisely where the X coordinates ended. */
    coord_cursor xc = { xcoords, 0 };
    decode_flags_and_coords(flags, 0, &xc, 0, X, on, total);
    coord_cursor yc = { xcoords + xc.cp, 0 };
    decode_flags_and_coords(flags, 0, &yc, 1, Y, on, total);
    return total;
}
