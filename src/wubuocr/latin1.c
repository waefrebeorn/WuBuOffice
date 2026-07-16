/* latin1.c -- Latin-1 (ISO-8859-1) glyph crowd (see latin1.h).
 * Clean C11, self-contained. */
#include "latin1.h"

#include <string.h>

/* Exported class tables (arrays of string pointers; last entry NULL). */
const char *OCR_ENGLISH_CHARS[95 + 1];
size_t      OCR_ENGLISH_N = 0;
const char *OCR_LATIN1_CHARS[96 + 1];
size_t      OCR_LATIN1_N = 0;

/* Build the tables once (on first use). */
static void ensure_tables(void) {
    if (OCR_ENGLISH_N) return;
    for (int c = 0x20; c <= 0x7E; c++) {
        static char buf[95][2];
        buf[c - 0x20][0] = (char)c;
        buf[c - 0x20][1] = '\0';
        OCR_ENGLISH_CHARS[c - 0x20] = buf[c - 0x20];
    }
    OCR_ENGLISH_CHARS[95] = NULL;
    OCR_ENGLISH_N = 95;
    /* Latin-1 0xA0..0xFF -> 2-byte UTF-8 (110xxxxx 10xxxxxx). */
    for (int c = 0xA0; c <= 0xFF; c++) {
        static char buf2[96][3];
        int u = c; /* Latin-1 codepoint == Unicode codepoint */
        buf2[c - 0xA0][0] = (char)(0xC0 | (u >> 6));
        buf2[c - 0xA0][1] = (char)(0x80 | (u & 0x3F));
        buf2[c - 0xA0][2] = '\0';
        OCR_LATIN1_CHARS[c - 0xA0] = buf2[c - 0xA0];
    }
    OCR_LATIN1_CHARS[96] = NULL;
    OCR_LATIN1_N = 96;
}

/* Force table population at load time (before any external reads them). */
static void __attribute__((constructor)) latin1_init(void) { ensure_tables(); }

int ocr_is_latin1(const char *s) {
    ensure_tables();
    if (!s) return 0;
    for (size_t i = 0; i < OCR_LATIN1_N; i++)
        if (strcmp(s, OCR_LATIN1_CHARS[i]) == 0) return 1;
    return 0;
}

size_t ocr_classes_english_latin(const char **out) {
    ensure_tables();
    size_t n = 0;
    for (size_t i = 0; i < OCR_ENGLISH_N; i++) out[n++] = OCR_ENGLISH_CHARS[i];
    for (size_t i = 0; i < OCR_LATIN1_N; i++) out[n++] = OCR_LATIN1_CHARS[i];
    return n;
}
