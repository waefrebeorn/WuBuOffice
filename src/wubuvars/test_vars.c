/* test_vars.c */
#include "vars.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Vars *v = vars_create();
    vars_set(v,"author","Alice"); vars_set(v,"year","2026");
    CK(strcmp(vars_get(v,"author"),"Alice")==0,"get");
    CK(vars_count(v)==2,"count");
    char *e = vars_expand(v,"by ${author} in ${year}");
    CK(e && strcmp(e,"by Alice in 2026")==0,"expand");
    free(e);
    char *u = vars_expand(v,"${missing} stays");
    CK(u && strcmp(u,"${missing} stays")==0,"unknown kept");
    free(u);
    vars_set(v,"author","Bob"); /* overwrite */
    CK(strcmp(vars_get(v,"author"),"Bob")==0,"overwrite");
    vars_destroy(v);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: vars (set/get/expand/overwrite)\n"); return 0;
}
