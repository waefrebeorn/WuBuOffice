/* test_caption.c */
#include "caption.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    CaptionMap *m = caption_create();
    CK(caption_set(m, 5, "Figure 1: logo")==1,"set");
    CK(caption_set(m, 7, "Table 1: data")==1,"set2");
    CK(strcmp(caption_get(m,5),"Figure 1: logo")==0,"get");
    CK(caption_get(m,1)==NULL,"unset");
    CK(caption_set(m,5,"Figure 1: revised")==1,"reset");
    CK(strcmp(caption_get(m,5),"Figure 1: revised")==0,"reset val");
    CK(caption_count(m)==2,"count");
    CK(caption_id_at(m,1)==7,"id at");
    caption_destroy(m);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: caption (set/get/reset/count)\n"); return 0;
}
