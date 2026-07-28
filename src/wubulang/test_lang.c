/* test_lang.c */
#include "lang.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    LangMap *m = lang_create();
    CK(lang_set(m, 10, "en")==1,"set");
    CK(lang_set(m, 20, "fr")==1,"set2");
    CK(strcmp(lang_get(m,10),"en")==0,"get");
    CK(lang_get(m,99)==NULL,"unset");
    CK(lang_set(m,10,"de")==1,"reset");
    CK(strcmp(lang_get(m,10),"de")==0,"reset val");
    CK(lang_count(m)==2,"count");
    CK(lang_id_at(m,0)==10 && strcmp(lang_tag_at(m,0),"de")==0,"at");
    lang_destroy(m);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: lang (set/get/reset/count)\n"); return 0;
}
