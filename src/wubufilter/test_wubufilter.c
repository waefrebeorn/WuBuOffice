#include "wubufilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

typedef struct { char *dept; int sales; } Row;
static const char *cell(void *row, int col, void *ud) {
    (void)ud; Row *r = (Row *)row;
    static char b[32];
    if (col == 0) return r->dept;
    snprintf(b, sizeof b, "%d", r->sales); return b;
}
static Row mk(const char *d, int s) { Row r; r.dept = strdup(d); r.sales = s; return r; }

int main(void) {
    Row rows[5] = { mk("A",10), mk("B",5), mk("A",30), mk("B",7), mk("C",15) };
    void *p[5] = { &rows[0],&rows[1],&rows[2],&rows[3],&rows[4] };
    size_t out[5], n;

    wubufilter_crit eq = {0, WUBUFILTER_EQ, "A"};
    CK(wubufilter_apply(p,5,&eq,1,cell,NULL,out,&n)==0 && n==2,"eq A");
    CK((out[0]==0&&out[1]==2),"eq indices");

    wubufilter_crit gt = {1, WUBUFILTER_GT, "6"};
    CK(wubufilter_apply(p,5,&gt,1,cell,NULL,out,&n)==0 && n==4,"gt 6");

    wubufilter_crit bt = {1, WUBUFILTER_BETWEEN, "6|15"};
    CK(wubufilter_apply(p,5,&bt,1,cell,NULL,out,&n)==0 && n==3,"between 6-15");
    /* sales 10,5,30,7,15 -> in [6,15]: row0(10), row3(7), row4(15) */
    CK(out[0]==0&&out[1]==3&&out[2]==4,"between indices");

    wubufilter_crit top = {1, WUBUFILTER_TOP, "2"};
    CK(wubufilter_apply(p,5,&top,1,cell,NULL,out,&n)==0 && n==2,"top 2");
    CK(out[0]==2,"top2 largest is A/30 (row2)");

    wubufilter_crit two[2] = { {0,WUBUFILTER_EQ,"B"}, {1,WUBUFILTER_LTE,"7"} };
    CK(wubufilter_apply(p,5,two,2,cell,NULL,out,&n)==0 && n==2,"B and sales<=7");
    CK((out[0]==1&&out[1]==3),"two-crit indices");

    int i; for(i=0;i<5;i++) free(rows[i].dept);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubufilter (eq/gt/between/top N/multi-criteria AutoFilter)\n");
    return 0;
}
