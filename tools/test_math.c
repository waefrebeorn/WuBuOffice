/* test_math.c -- verifies the math/equation line detector (#48). Feeds known
 * math and prose strings and asserts the classifier separates them. */
#include "crnn_transcribe.h"
#include <stdio.h>
#include <string.h>

struct { const char *s; int expect; } cases[] = {
    /* math: should be detected */
    {"E = mc^2", 1},
    {"a^2 + b^2 = c^2", 1},
    {"x = (-b + sqrt(b^2 - 4ac)) / (2a)", 1},
    {"H_2O + O_2 -> H_2O_2", 1},
    {"f(x) = (a + (b * c)) / d", 1},
    {"\xCE\xB1 + \xCE\xB2 = \xCE\xB3", 1},          /* α + β = γ (Greek) */
    {"\xE2\x88\xAB f(x) dx = 0", 1},                /* ∫ f(x) dx */
    {"\xE2\x88\x91 x_i = N", 1},                    /* ∑ x_i */
    /* prose: should NOT be detected */
    {"the quick brown fox jumps", 0},
    {"This is a normal sentence about documents.", 0},
    {"We the people of the United States", 0},
    {"a b c d e f g", 0},
    {"hello world", 0},
    {NULL, 0}
};

int main(void){
    int fail = 0;
    for (int i=0; cases[i].s; i++){
        int got = wubuocr_detect_math_line(cases[i].s);
        int ok = (got == cases[i].expect);
        if (!ok) fail++;
        printf("%s  got=%d expect=%d  \"%s\"\n", ok?"ok ":"FAIL", got, cases[i].expect, cases[i].s);
    }
    printf(fail ? "FAIL: %d math cases wrong\n" : "PASS: all math cases correct\n", fail);
    return fail ? 1 : 0;
}
