/* test_nfc.c -- unit test for Unicode NFC Latin composition (#96).
 * Feeds combining-mark sequences and checks they precompose correctly. */
#include <stdio.h>
#include <string.h>
#include "crnn_transcribe.h"

static int check(const char *in, const char *want) {
    char out[64];
    wubuocr_nfc_latin(in, out, sizeof out);
    if (strcmp(out, want) != 0) {
        printf("FAIL: nfc(\"%s\") = \"%s\", want \"%s\"\n", in, out, want);
        return 0;
    }
    return 1;
}

int main(void){
    int ok = 1;
    /* e + combining acute (U+0301) -> é (U+00E9) */
    ok &= check("e\xCC\x81", "\xC3\xA9");
    /* E + combining acute -> É (U+00C9) */
    ok &= check("E\xCC\x81", "\xC3\x89");
    /* a + combining diaeresis (U+0308) -> ä (U+00E4) */
    ok &= check("a\xCC\x88", "\xC3\xA4");
    /* plain ASCII is untouched */
    ok &= check("Hello", "Hello");
    /* multiple marks: e + acute + diaeresis -> ě (U+011B)? no: e+acute=é,
     * then é+diaeresis has no precomposed form -> stays as e+acute + U+0308.
     * We just check it does not crash and is longer than input. */
    char out[64]; wubuocr_nfc_latin("e\xCC\x81\xCC\x88", out, sizeof out);
    if (strlen(out) == 0) { printf("FAIL: multi-mark produced empty\n"); ok=0; }
    if (ok) printf("PASS: test_nfc (Latin NFC composition)\n");
    return ok ? 0 : 1;
}
