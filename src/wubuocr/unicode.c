/* unicode.c -- generalized Unicode OCR token classes (see unicode.h).
 * Clean C11, self-contained. */
#include "unicode.h"

#include <string.h>

/* Encode a Unicode codepoint as UTF-8 into a 4-byte static buffer (NUL
 * terminated). Returns the byte length (1..3). */
static int put_utf8(char *buf, unsigned cp) {
    if (cp <= 0x7F) {
        buf[0] = (char)cp; buf[1] = '\0'; return 1;
    }
    if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        buf[2] = '\0'; return 2;
    }
    /* BMP 3-byte */
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    buf[3] = '\0'; return 3;
}

/* A curated "common" Asian token set: the scripts that ARE token-shaped
 * (one glyph == one token) and widely used, omitting Arabic/connected scripts.
 * We take the first N codepoints of each block as a representative, scalable
 * set -- a deployment with a massive font library can raise the bounds. */
typedef struct { unsigned first, last; } Block;

static const Block CJK_BLOCKS[] = {
    { 0x3000, 0x303F }, /* CJK Symbols and Punctuation */
    { 0x3040, 0x309F }, /* Hiragana */
    { 0x30A0, 0x30FF }, /* Katakana */
    { 0x3100, 0x312F }, /* Bopomofo */
    { 0x4E00, 0x9FFF }, /* CJK Unified Ideographs (core) */
    { 0xAC00, 0xD7A3 }, /* Hangul Syllables */
};
static const size_t CJK_BLOCK_N = sizeof(CJK_BLOCKS) / sizeof(CJK_BLOCKS[0]);

/* Cap per block so the default bank stays tractable but clearly "Asian". */
static const unsigned BLOCK_CAP = 256;

size_t ocr_unicode_cjk_count(void) {
    size_t n = 0;
    for (size_t b = 0; b < CJK_BLOCK_N; b++) {
        unsigned span = CJK_BLOCKS[b].last - CJK_BLOCKS[b].first + 1;
        unsigned take = span < BLOCK_CAP ? span : BLOCK_CAP;
        n += take;
    }
    return n;
}

size_t ocr_unicode_base_count(void) {
    return 95 /* ASCII 0x20..0x7E */ + 96 /* Latin-1 0xA0..0xFF */;
}

size_t ocr_unicode_classes(const char **out, size_t out_cap, int include_cjk) {
    if (!out || out_cap == 0) return 0;
    size_t n = 0;
    /* English-first: printable ASCII. */
    for (int c = 0x20; c <= 0x7E && n + 1 < out_cap; c++) {
        static char eb[95][2];
        eb[c - 0x20][0] = (char)c;
        eb[c - 0x20][1] = '\0';
        out[n++] = eb[c - 0x20];
    }
    /* Latin-1 supplement (omit C1 controls 0x80..0x9F). */
    for (int c = 0xA0; c <= 0xFF && n + 1 < out_cap; c++) {
        static char lb[96][3];
        put_utf8(lb[c - 0xA0], (unsigned)c);
        out[n++] = lb[c - 0xA0];
    }
    /* Tokenized Asian blocks (omitting Arabic/connected scripts). */
    if (include_cjk) {
        for (size_t b = 0; b < CJK_BLOCK_N; b++) {
            unsigned span = CJK_BLOCKS[b].last - CJK_BLOCKS[b].first + 1;
            unsigned take = span < BLOCK_CAP ? span : BLOCK_CAP;
            for (unsigned i = 0; i < take && n + 1 < out_cap; i++) {
                static char kb[2048][4];
                unsigned cp = CJK_BLOCKS[b].first + i;
                put_utf8(kb[n], cp);
                out[n] = kb[n];
                n++;
            }
        }
    }
    out[n] = NULL;
    return n;
}
