/* test_scope.c */
#include "scope.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    ScopeMap *m = scope_create();
    CK(scope_set(m, 3, "col")==1,"set col");
    CK(scope_set(m, 4, "row")==1,"set row");
    CK(scope_set(m, 5, "bogus")==1,"bad -> default col");
    CK(strcmp(scope_get(m,3),"col")==0,"get col");
    CK(strcmp(scope_get(m,4),"row")==0,"get row");
    CK(strcmp(scope_get(m,5),"col")==0,"bad defaulted");
    CK(scope_get(m,9)==NULL,"unset");
    CK(scope_count(m)==3,"count");
    scope_destroy(m);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: scope (col/row/default/lookup)\n"); return 0;
}
