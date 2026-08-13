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

    /* REAL engine: route produces a valid 3-point elbow from A to B. */
    wubuc_rect a = { 10, 10, 100, 40 };  /* A */
    wubuc_rect b = { 300, 200, 100, 40 }; /* B */
    float p[6];
    CK(wubuconnector_route(c, 0, &a, &b, p) == 0, "route A->B");
    /* start = A right-center */
    CK(p[0] == a.x + a.w && p[1] == a.y + a.h*0.5f, "start at A right edge");
    /* elbow: horizontal run at A's mid-height, reaching B's left edge x */
    CK(p[2] == b.x, "elbow x reaches target left edge (clean horizontal run)");
    CK(p[3] == a.y + a.h*0.5f, "elbow horizontal run at A's mid");
    /* end = B left-center, reached by a vertical drop sharing elbow x */
    CK(p[4] == b.x && p[5] == b.y + b.h*0.5f, "end at B left edge");
    CK(p[2] == p[4], "vertical drop shares elbow x (clean L)");

    CK(wubuconnector_route(c, 99, &a, &b, p) == -1, "bad index");

    wubuconnector_destroy(c);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuconnector (diagram connector model + orthogonal L-route)\n");
    return 0;
}
