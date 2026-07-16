/* unicode.h -- generalized, language-agnostic OCR token classes.
 *
 * The design decision (per the user): OCR is language-agnostic. A glyph is
 * just a shape that maps to a UTF-8 codepoint token plus coordinates; there
 * is NO language model and NO translation in this layer (those are a separate
 * AI problem). So "extended Unicode" is simply: more glyph token classes.
 *
 * Token ordering is ENGLISH-FIRST (printable ASCII), then Latin-1, then
 * tokenized Asian blocks (Hiragana/Katakana/common CJK ideographs/Hangul).
 * Arabic and other connected/right-to-left scripts are intentionally OMITTED
 * -- they are not token-shaped (single glyph = one token) and belong to a
 * different engine. Every codepoint here is still just one OCR token.
 *
 * The class strings produced are NUL-terminated UTF-8 (1/2/3 bytes) suitable
 * for ocr_fontbank_build / ocr_compose_page directly.
 */
#ifndef WUBUOCR_UNICODE_H
#define WUBUOCR_UNICODE_H

#include <stddef.h>

/* Fill `out` (capacity `out_cap` pointers) with the ordered Unicode token
 * class set. Returns the number of classes written (including English +
 * Latin-1; CJK common subset only if `include_cjk` is non-zero). `out[count]`
 * is set to NULL. `out_cap` should be large enough; typical callers pass a
 * few thousand. Returns 0 on NULL input / bad capacity. */
size_t ocr_unicode_classes(const char **out, size_t out_cap, int include_cjk);

/* Number of English (ASCII) + Latin-1 classes, the always-on base. */
size_t ocr_unicode_base_count(void);

/* Number of CJK/common Asian token classes added when include_cjk is set. */
size_t ocr_unicode_cjk_count(void);

#endif /* WUBUOCR_UNICODE_H */
