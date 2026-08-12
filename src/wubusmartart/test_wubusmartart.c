#include "wubusmartart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubusmartart *s = wubusmartart_create();
    CK(wubusmartart_layout(s)==WUBU_SA_PROCESS, "default process");
    CK(wubusmartart_set_layout(s,WUBU_SA_CYCLE)==0 && wubusmartart_layout(s)==WUBU_SA_CYCLE, "set cycle");
    CK(wubusmartart_add_node(s,"Plan")==0, "add Plan");
    CK(wubusmartart_add_node(s,"Do")==0, "add Do");
    CK(wubusmartart_add_node(s,"Check")==0, "add Check");
    CK(wubusmartart_count(s)==3, "3 nodes");
    CK(strcmp(wubusmartart_node(s,2),"Check")==0, "node 2");
    CK(wubusmartart_node(s,9)==NULL, "out of range");
    CK(wubusmartart_set_layout(s,99)==-1, "reject bad layout");

    wubusmartart_destroy(s);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusmartart (diagram layouts: process/cycle/hierarchy/list)\n");
    return 0;
}
