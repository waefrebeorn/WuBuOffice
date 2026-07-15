/* font8x8.h -- embedded public-domain 8x8 bitmap font (template source).
 *
 * 128 glyphs (U+0000..U+007F), 8 bytes each (one byte per row, top->bottom).
 * Within a row byte the LEAST-significant bit is the LEFTMOST pixel; a set bit
 * is ink. This is the reference glyph set the zoning 1-NN recognizer measures
 * candidate glyphs against -- a deterministic, dependency-free template bank
 * (no training data, no model weights).
 *
 * Public Domain (Marcel Sondaar / IBM VGA fonts; C array by Daniel Hepper).
 */
#ifndef WUBUOCR_FONT8X8_H
#define WUBUOCR_FONT8X8_H

#ifdef __cplusplus
extern "C" {
#endif

extern const unsigned char wubuocr_font8x8[128][8];

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_FONT8X8_H */
