#include "wubuconnector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuconnector *c = wubuconnector_create();
    CK(wubuconnector_add(c,"A","out","B","in") == 0, "add A->B");
    CK(wubuconnector_add(c,"B","out","C","in") == 0, "add B->C");
    CK(wubuconnector_count(c) == 2, "2 connectors");
    CK(strcmp(wubuconnector_from(c,0),"A")==0 && strcmp(wubuconnector_to(c,0),"B")==0, "first conn");
    CK(wubuconnector_to(c,5) == NULL, "out of range");
    CK(wubuconnector_add(c,NULL,"out","B","in") == -1, "reject null from");

    wubuconnector_destroy(c);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuconnector (diagram connector edge model)\n");
    return 0;
}
