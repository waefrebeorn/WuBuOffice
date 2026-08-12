#include "wubuanimation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuanimation *a = wubuanimation_create();
    CK(wubuanimation_add(a,"title",WUBU_AN_FADE,1.0,0.0,0) == 0, "add fade");
    CK(wubuanimation_add(a,"pic",WUBU_AN_FLYIN,1.5,0.5,3) == 0, "add flyin");
    CK(wubuanimation_count(a) == 2, "2 keys");

    const wubuan_key *k = wubuanimation_get(a,1);
    CK(k && strcmp(k->target,"pic")==0 && k->type==WUBU_AN_FLYIN && k->repeat==3, "key values");
    CK(wubuanimation_get(a,9) == NULL, "out of range");
    CK(wubuanimation_add(a,"x",WUBU_AN_SPIN,-1,0,0) == -1, "reject neg dur");

    wubuanimation_destroy(a);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuanimation (per-object keyframe animations)\n");
    return 0;
}
