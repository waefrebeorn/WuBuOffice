/* test_rtf.c */
#include "rtf.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    RtfRun runs[2] = {
        { "Bold ", 1, 0, 0 },
        { "italic {x}", 0, 1, 0 },
    };
    char *rtf = rtf_write(runs, 2);
    CK(rtf != NULL,"write");
    if (rtf){
        CK(strstr(rtf,"\\rtf1")!=NULL,"header");
        CK(strstr(rtf,"\\b")!=NULL,"bold tag");
        CK(strstr(rtf,"\\i")!=NULL,"italic tag");
        CK(strstr(rtf,"\\{x\\}")!=NULL,"brace escaped");
        CK(rtf[0]=='{' && rtf[strlen(rtf)-1]=='}',"wrapped");
        free(rtf);
    }
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: rtf (bold/italic + brace escaping)\n"); return 0;
}
