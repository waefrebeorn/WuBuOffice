/* latin1.h -- Latin-1 (ISO-8859-1) glyph crowd for multilingual OCR second.
 *
 * The user's requirement: "English first and Latin language second". The
 * bank/recognizer core targets printable ASCII (English). This module supplies
 * the Latin-1 tier (0xA0..0xFF) as a crowd of single-codepoint UTF-8 strings,
 * ordered English-first (the ASCII letters stay the primary classes; accented
 * Latin letters are added as a secondary tier). A composite class list is built
 * by concatenating the English tier with the Latin tier.
 *
 * Self-contained, no wubufont dependency (it is just codepoint knowledge).
 * Opaque-free: pure data + helpers. */
#ifndef WUBUOCR_LATIN1_H
#define WUBUOCR_LATIN1_H

#include <stddef.h>

/* English-first class set: printable ASCII 0x20..0x7E. */
extern const char *OCR_ENGLISH_CHARS[];   /* NULL-terminated */
extern size_t      OCR_ENGLISH_N;

/* Latin-1 tier: the accented/extended glyphs 0xA0..0xFF, each as a UTF-8
 * string (1 or 2 bytes). NULL-terminated. */
extern const char *OCR_LATIN1_CHARS[];
extern size_t      OCR_LATIN1_N;

/* Is `s` (a 1- or 2-byte UTF-8 glyph string) a member of the Latin-1 tier? */
int ocr_is_latin1(const char *s);

/* Build a composite English-first + Latin class set into `out` (caller
 * provides an array of >= OCR_ENGLISH_N + OCR_LATIN1_N pointers). Returns the
 * count written. English classes occupy [0, OCR_ENGLISH_N); Latin occupy
 * [OCR_ENGLISH_N, ...). */
size_t ocr_classes_english_latin(const char **out);

#endif /* WUBUOCR_LATIN1_H */
