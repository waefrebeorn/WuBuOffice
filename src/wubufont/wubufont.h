/* wubufont.h -- clean-room SFNT/TrueType/OpenType parser (subset).
 *
 * Scope: enough of ISO/IEC 14496-22 (Open Font Format = TrueType/OTF) to
 * drive the WuBuOffice/WuBuPad backbone: read an sfnt container, enumerate
 * its tables, decode glyph outlines (quadratic TrueType 'glyf'), map
 * code points via 'cmap' (format 4), read naming + header metrics, and
 * emit a glyph outline as an SVG path. No rendering, no hinting, no
 * dependency on freetype/opentype.js. Native C11.
 *
 * Reference standards:
 *   - ISO/IEC 14496-22 : Open Font Format (sfnt, glyf, cmap, head, hhea, ...)
 *   - Microsoft OpenType spec (cmap subtable formats, name table)
 */
#ifndef WUBUFONT_H
#define WUBUFONT_H

#include <stddef.h>
#include <stdint.h>

typedef struct Font Font;

/* Open a font from an in-memory sfnt blob. Returns NULL if the signature is
 * not a recognised sfnt ('true'/'OTTO'/0x00010000). The blob must outlive
 * the Font (we reference it, we do not copy it). */
Font *font_open(const uint8_t *data, size_t size);
/* Like font_open, but if take_ownership != 0 the blob is copied so the
 * returned Font is fully self-contained (used by woff_open). */
Font *font_open_owned(const uint8_t *data, size_t size, int take_ownership);
void  font_free(Font *f);

/* number of tables in the sfnt directory */
size_t font_table_count(const Font *f);
/* table tag (big-endian 4cc) and byte range; idx in [0,font_table_count). */
uint32_t font_table_tag(const Font *f, size_t idx);
int      font_table_range(const Font *f, size_t idx, size_t *offset, size_t *length);
/* find a table by tag ('cmap','glyf','head','hhea','name','maxp','loca'...).
 * returns 1 if present, fills offset/length. */
int font_find_table(const Font *f, uint32_t tag, size_t *offset, size_t *length);

/* unitsPerEm from 'head' (0 if absent) */
uint16_t font_units_per_em(const Font *f);
/* number of glyphs from 'maxp' (0 if absent) */
uint16_t font_glyph_count(const Font *f);
/* ascent/descent from 'hhea' (0 if absent) */
int16_t  font_ascent(const Font *f);
int16_t  font_descent(const Font *f);

/* Naming: pull a string from the 'name' table. platform=3 (Windows,
 * UTF-16BE), encoding=1, language=0x409 (en-US) is tried first, then any.
 * name_id: 1 family, 2 subfamily, 4 full name, 6 postscript name.
 * Returns a NUL-terminated malloc'd UTF-8 string (caller frees) or NULL. */
char *font_name(const Font *f, uint16_t name_id);

/* cmap: map a Unicode code point to a glyph index (format 4 preferred).
 * returns 0 if not present. */
uint16_t font_cmap(const Font *f, uint32_t codepoint);

/* Advance width (font units) for a Unicode code point via 'hmtx'.
 * Returns -1 if the font has no hmtx/cmap. */
int font_advance(const Font *f, uint32_t codepoint);

/* Glyph outline as an SVG path 'd' string. Flips Y so the coordinate system
 * matches SVG (glyph space has +Y up). Returns a malloc'd string (caller
 * frees) or NULL on failure. contours are separated by Z. */
char *font_glyph_svg_path(const Font *f, uint16_t glyph_index);

/* Emit a standalone SVG <font> document for the font, including a sample
 * string rendered as <text> (so the SVG is non-empty and convertible).
 * Returns a malloc'd NUL-terminated SVG document (caller frees) or NULL. */
char *font_to_svg(const Font *f, const char *sample);

/* ---- raw glyph outline access (for rasterization, no SVG string) ---- */
/* A contour is a list of points; on_curve marks TrueType on-curve points
 * (off-curve points are quadratic Bézier control points). */
typedef struct { int x, y; int on_curve; } FontPoint;
typedef struct {
    FontPoint *pts;
    size_t     n;
    int        empty;   /* 1 if the glyph has no ink (space, etc.) */
} FontContour;

/* Decode a code point into one or more contours (composite glyphs expanded).
 * Returns the number of contours filled (caller frees each .pts with free()),
 * or 0 if the code point is unmapped / has no ink. `scale` maps font units
 * (em-square) to output pixels (e.g. pixels_per_em / units_per_em). */
size_t font_glyph_contours(const Font *f, uint32_t codepoint,
                           double scale, FontContour *out, size_t max_out);

/* Rasterize a code point into a 1-bit (ink) bitmap. Allocates *bits
 * (caller frees) as (w*h) bytes, 1 = ink. Returns 1 on success, 0 on failure
 * or if the glyph has no ink (in which case *w=*h=0). `ppm` = pixels per em. */
int font_rasterize(const Font *f, uint32_t codepoint, int ppm,
                   uint8_t **bits, int *w, int *h);

/* Rasterize a string into a single row bitmap (glyphs left-to-right, no
 * kerning beyond advance widths). Allocates *bits (caller frees) of size
 * (*w)*(*h); 1 = ink. Returns 1 on success. */
int font_rasterize_string(const Font *f, const char *utf8, int ppm,
                          uint8_t **bits, int *w, int *h);

#endif /* WUBUFONT_H */
