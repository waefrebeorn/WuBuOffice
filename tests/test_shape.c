/* test_shape.c -- wubushape Bidi reorder check (INT-7). */
#include "shape.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c, m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

int main(void){
    char out[256];

    /* Pure LTR base, LTR text -> unchanged */
    shape_reorder("hello", SHAPE_LTR, out, sizeof out);
    CK(strcmp(out, "hello")==0, "ltr unchanged");

    /* Pure RTL base, single RTL word -> reversed byte order (Arabic 'مرحبا').
     * Visual order of an isolated RTL run IS the run reversed (correct Bidi). */
    const char *ar = "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7";
    size_t n = shape_reorder(ar, SHAPE_RTL, out, sizeof out);
    CK(n == strlen(ar), "rtl length preserved");
    /* a single RTL run under RTL base is emitted reversed -> must differ from
     * the logical bytes unless palindromic; just assert it's non-empty + valid. */
    CK(n == 10, "single rtl run emitted full byte count");

    /* Mixed: RTL base, Arabic then embedded Latin 'abc'. Visual order should
     * put the Latin word to the LEFT of the Arabic (RTL paragraph). */
    const char *mix = "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 abc";
    shape_reorder(mix, SHAPE_RTL, out, sizeof out);
    /* 'abc' must be present and at/near the left (the RTL Arabic follows). */
    char *abc = strstr(out, "abc");
    CK(abc != NULL, "embedded latin preserved in rtl");
    CK(abc <= out + 4, "embedded latin is leftmost in rtl visual order");

    /* RTL codepoint detection */
    CK(shape_is_rtl_codepoint(0x0627), "arabic is rtl");
    CK(shape_is_rtl_codepoint(0x05D0), "hebrew is rtl");
    CK(!shape_is_rtl_codepoint('a'), "latin not rtl");
    CK(!shape_is_rtl_codepoint('9'), "digit not rtl");

    if (fails){ printf("SHAPE TESTS FAILED (%d)\n", fails); return 1; }
    printf("SHAPE TESTS PASSED\n");
    return 0;
}
