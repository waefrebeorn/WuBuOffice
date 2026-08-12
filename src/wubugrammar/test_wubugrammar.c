#include "wubugrammar.h"
#include <stdio.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubugrammar_finding f[32];
    /* repeated word + misspelling + a-before-vowel + an-before-consonant */
    const char *t1 = "this is is alot of fun and a apple an book";
    int n = wubugrammar_check(t1, f, 32);
    CK(n >= 4, "multiple findings");
    int has_double=0, has_misspell=0, has_aan=0;
    for (int i=0;i<n;i++) {
        if (f[i].issue_id==1) has_double=1;
        if (f[i].issue_id==4 && f[i].message[0]=='\'') has_misspell=1;
        if (f[i].issue_id==2 || f[i].issue_id==3) has_aan=1;
    }
    CK(has_double,"repeated word detected");
    CK(has_misspell,"misspelling detected (alot)");
    CK(has_aan,"a/an rule detected");

    /* clean text: minimal findings */
    const char *t2 = "The quick brown fox jumps over the lazy dog";
    int n2 = wubugrammar_check(t2, f, 32);
    CK(n2 == 0, "clean text no findings");

    /* double space */
    const char *t3 = "hello  world";
    int n3 = wubugrammar_check(t3, f, 32);
    CK(n3 >= 1 && f[0].issue_id==5, "double space detected");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubugrammar (doubled word, a/an, misspellings, double space)\n");
    return 0;
}
