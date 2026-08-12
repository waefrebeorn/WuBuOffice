#include "wubunotebookbar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubunotebookbar *n = wubunotebookbar_create();
    CK(wubunotebookbar_add(n,"Sheet1")==0, "add Sheet1");
    CK(wubunotebookbar_add(n,"Sheet2")==0, "add Sheet2");
    CK(wubunotebookbar_add(n,"Sheet3")==0, "add Sheet3");
    CK(wubunotebookbar_count(n)==3, "3 tabs");
    CK(strcmp(wubunotebookbar_name(n,1),"Sheet2")==0, "tab 1 name");
    CK(wubunotebookbar_active(n)==0, "default active 0");
    CK(wubunotebookbar_set_active(n,2)==0 && wubunotebookbar_active(n)==2, "set active 2");
    CK(wubunotebookbar_set_active(n,9)==-1, "reject out of range");

    wubunotebookbar_destroy(n);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubunotebookbar (spreadsheet sheet-tab strip)\n");
    return 0;
}
