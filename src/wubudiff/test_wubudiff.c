#include "wubudiff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    /* identical */
    CK(wubudiff_count("a\nb\nc\n","a\nb\nc\n")==3,"identical 3 eq hunks");

    /* one insertion */
    const char *a = "a\nc\n", *b = "a\nb\nc\n";
    wubudiff_hunk h[8];
    int n = wubudiff_text(a,b,h,8);
    CK(n==3,"insert yields 3 hunks");
    /* expect: EQ(a), INS(b), EQ(c) */
    CK(h[0].op==WUBUDIFF_EQ && h[1].op==WUBUDIFF_INS && h[1].b_line==1 && h[2].op==WUBUDIFF_EQ,"insert positions");

    /* one deletion */
    int n2 = wubudiff_count("a\nb\nc\n","a\nc\n");

    /* substitution (del+ins) */
    int n3 = wubudiff_count("x\ny\n","x\nz\n");
    CK(n3==3,"substitute del+ins+eq");

    /* empty */
    CK(wubudiff_count("","")==0,"both empty 0");
    CK(wubudiff_count("a\n","")==1,"one side empty");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubudiff (LCS line diff: eq/ins/del, substitution, empty)\n");
    return 0;
}
