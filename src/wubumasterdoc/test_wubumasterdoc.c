#include "wubumasterdoc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubumasterdoc *m = wubumasterdoc_create();
    CK(wubumasterdoc_add(m,"ch1.docx")==0 && wubumasterdoc_add(m,"ch3.docx")==0,"add 2");
    CK(wubumasterdoc_insert(m,1,"ch2.docx")==0,"insert ch2 at 1");
    CK(wubumasterdoc_count(m)==3,"count 3");
    CK(strcmp(wubumasterdoc_get(m,0),"ch1.docx")==0 && strcmp(wubumasterdoc_get(m,1),"ch2.docx")==0
       && strcmp(wubumasterdoc_get(m,2),"ch3.docx")==0,"ordered paths");

    CK(wubumasterdoc_remove(m,1)==1,"remove ch2");
    CK(wubumasterdoc_count(m)==2,"count 2 after remove");
    CK(strcmp(wubumasterdoc_get(m,1),"ch3.docx")==0,"reordered after remove");
    CK(wubumasterdoc_remove(m,5)==0,"out-of-range remove no-op");
    CK(wubumasterdoc_get(m,9)==NULL,"out-of-range get NULL");
    CK(wubumasterdoc_insert(m,9,"x.docx")==-1,"insert past end rejected");

    wubumasterdoc_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumasterdoc (ordered sub-doc references, insert/remove/enum)\n");
    return 0;
}
