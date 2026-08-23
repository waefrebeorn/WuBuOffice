/* wubufont.c -- unity aggregator for the clean-room SFNT/TrueType module.
 *
 * This file is intentionally tiny. All real logic lives in focused,
 * self-contained fragments under src/wubufont/ so each concern (sfnt
 * container parsing, name-table decode, cmap lookup, glyf contour decode,
 * SVG emission, rasterization) is readable and reviewable on its own.
 *
 * wubufont_internal.h defines the opaque-to-callers `Font` struct plus the
 * shared big-endian readers, glyf flag-bit macros, the coord_cursor type, and
 * the forward declarations of the module-private `static` helpers the
 * fragments hand to one another. Because every fragment #includes it and it
 * is #include-guarded, the shared decode_simple_points path is defined exactly
 * once (DRY: the SVG emitter and the rasterizer reuse the identical contour
 * decoder). See wubufont.h for the public API.
 */
#include "wubufont_internal.h"

/* --- sfnt container: open/free, directory, table lookup, metrics --- */
#include "font_parse.inc"
/* --- name table -> UTF-8 (font_name) --- */
#include "font_name.inc"
/* --- cmap -> glyph index (format 0/4/6) --- */
#include "font_cmap.inc"
/* --- glyf: shared quadratic-Bézier contour decoder (decode_simple_points,
 *     coord_next, decode_flags_and_coords) reused by SVG + raster --- */
#include "font_glyf_decode.inc"
/* --- glyph outline -> SVG path 'd' and full <font> document --- */
#include "font_svg.inc"
/* --- raw contour access + even-odd rasterizer (bitmap output) --- */
#include "font_raster.inc"

/* ---- public: advance width via hmtx (hop 15) ---- */
int font_advance(const Font *f, uint32_t codepoint) {
    if (!f) return -1;
    size_t ho, hl;
    if (!font_find_table(f, TAG('h','m','t','x'), &ho, &hl)) return -1;
    uint16_t gi = font_cmap(f, codepoint);
    uint16_t ng = font_glyph_count(f);
    if (gi >= ng) return -1;
    const uint8_t *hm = f->data + ho;
    return rd16(hm + (size_t)gi * 4);
}
