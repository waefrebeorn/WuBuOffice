#define _GNU_SOURCE
/* test_xps.c */
#include "xps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    uint8_t *buf; size_t len;
    CK(xps_build("Hello XPS", 816, 1056, &buf, &len)==1,"build");
    CK(buf && len>0,"non-empty");
    if (buf){
        /* ZIP local-file-header signature */
        CK(buf[0]=='P' && buf[1]=='K' && buf[2]==0x03 && buf[3]==0x04,"zip sig");
        /* contains a FixedPage with the text */
        CK(memmem(buf, len, "FixedPage", 8)!=NULL,"fixedpage present");
        CK(memmem(buf, len, "Hello XPS", 9)!=NULL,"text present");
        free(buf);
    }
    CK(xps_write_file("/tmp/test.xps", "doc", 816, 1056)==0,"write file");
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: xps (store-ZIP container + FixedPage)\n"); return 0;
}
