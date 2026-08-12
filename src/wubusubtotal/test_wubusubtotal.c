#include "wubusubtotal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

typedef struct { char *dept; int sales; } Row;
static const char *key(void *row, void *ud) { (void)ud; return ((Row*)row)->dept; }
static const char *val(void *row, void *ud) {
    (void)ud; static char b[32]; snprintf(b,sizeof b,"%d",((Row*)row)->sales); return b;
}
static Row mk(const char *d, int s) { Row r; r.dept=strdup(d); r.sales=s; return r; }

int main(void) {
    Row rows[5] = { mk("A",10), mk("B",5), mk("A",30), mk("B",7), mk("C",15) };
    void *p[5] = { &rows[0],&rows[1],&rows[2],&rows[3],&rows[4] };
    wubusub_group *g; size_t n;

    CK(wubusub_aggregate(p,5,key,val,WUBUSUB_SUM,NULL,&g,&n)==0 && n==3,"3 groups");
    /* first-seen order A,B,C */
    CK(strcmp(g[0].group,"A")==0 && strcmp(g[1].group,"B")==0 && strcmp(g[2].group,"C")==0,"order");
    CK(wubusub_value(&g[0],WUBUSUB_SUM)==40 && wubusub_value(&g[1],WUBUSUB_SUM)==12,"sums");
    CK(wubusub_value(&g[0],WUBUSUB_AVG)==20 && wubusub_value(&g[0],WUBUSUB_COUNT)==2,"avg/count");
    CK(wubusub_value(&g[0],WUBUSUB_MIN)==10 && wubusub_value(&g[0],WUBUSUB_MAX)==30,"min/max");

    /* count-only groups by key ignoring value */
    wubusub_group *g2; size_t n2;
    CK(wubusub_aggregate(p,5,key,val,WUBUSUB_COUNT,NULL,&g2,&n2)==0 && n2==3,"count 3 groups");
    CK(wubusub_value(&g2[0],WUBUSUB_COUNT)==2 && wubusub_value(&g2[2],WUBUSUB_COUNT)==1,"counts");

    wubusub_free(g,n); wubusub_free(g2,n2);
    int i; for(i=0;i<5;i++) free(rows[i].dept);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusubtotal (group-by key, SUM/AVG/COUNT/MIN/MAX)\n");
    return 0;
}
