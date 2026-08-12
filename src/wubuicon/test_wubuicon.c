#include "wubuicon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuicon *i = wubuicon_create();
    CK(wubuicon_add(i,"save","M5 5h14v14H5z") == 0, "add save");
    CK(wubuicon_add(i,"open","M3 8l5-4h13v16H3z") == 0, "add open");
    CK(wubuicon_count(i) == 2, "count 2");

    CK(strcmp(wubuicon_get(i,"save"),"M5 5h14v14H5z") == 0, "get save");
    CK(wubuicon_get(i,"nope") == NULL, "absent icon");
    CK(strcmp(wubuicon_name(i,0),"save") == 0 && strcmp(wubuicon_name(i,1),"open") == 0, "enum names");

    /* overwrite */
    CK(wubuicon_add(i,"save","M0 0h20v20H0z") == 0, "overwrite save");
    CK(strcmp(wubuicon_get(i,"save"),"M0 0h20v20H0z") == 0, "overwritten data");
    CK(wubuicon_count(i) == 2, "count still 2");

    wubuicon_destroy(i);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuicon (named icon registry, add/get/overwrite/enum)\n");
    return 0;
}
