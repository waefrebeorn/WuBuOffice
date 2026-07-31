/* test_redact.c */
#include "redact.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Redact *r = redact_create();
    redact_mark(r, 0, 6);    /* hides "Secret" */
    redact_mark(r, 9, 13);   /* hides "1234"   */
    CK(redact_count(r)==2,"count");
    char *out = redact_apply(r, "SecretSSN1234");
    CK(out && strcmp(out,"██████SSN████")==0,"apply");
    free(out);
    CK(redact_apply(r, "short")==NULL,"oob null");
    redact_destroy(r);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: redact (mark/apply/out-of-bounds)\n"); return 0;
}
