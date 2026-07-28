/* test_pasteplain.c */
#include "pasteplain.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    char *h = pasteplain_strip("<b>Hello</b> world");
    CK(h && strcmp(h,"Hello world")==0,"html strip");
    free(h);
    char *r = pasteplain_strip("\\b bold \\i italic \\par end");
    CK(r && strcmp(r,"bold italic end")==0,"rtf strip");
    free(r);
    char *p = pasteplain_strip("already plain");
    CK(p && strcmp(p,"already plain")==0,"plain passthrough");
    free(p);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: pasteplain (html/rtf strip)\n"); return 0;
}
