#include "wubuindex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuindex *ix = wubuindex_create();
    CK(wubuindex_add_term(ix,"Banana")==0 && wubuindex_add_term(ix,"Apple")==0 && wubuindex_add_term(ix,"apple")==0,"add terms");
    CK(wubuindex_count(ix)==2,"dedup case-insensitive to 2");

    /* pages: apple on 1,3; banana on 2 */
    CK(wubuindex_feed_page(ix,"The Apple falls",1)==0,"page1 apple");
    CK(wubuindex_feed_page(ix,"A banana ripens and another apple",2)==0,"page2");
    CK(wubuindex_feed_page(ix,"Apple pie",3)==0,"page3 apple");

    const wubuindex_entry *apple = wubuindex_get(ix,0);
    const wubuindex_entry *banana = wubuindex_get(ix,1);
    CK(strcmp(apple->term,"apple")==0 && strcmp(banana->term,"banana")==0,"sorted terms");
    CK(apple->npages==3 && apple->pages[0]==1 && apple->pages[1]==2 && apple->pages[2]==3,"apple pages sorted");
    CK(banana->npages==1 && banana->pages[0]==2,"banana page");

    wubuindex_destroy(ix);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuindex (vocab->sorted page refs, dedup, case-insensitive)\n");
    return 0;
}
