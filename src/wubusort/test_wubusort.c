#include "wubusort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

typedef struct { char *name; char age[16]; char score[16]; } Row;
static const char *cell(void *row, int col, void *ud) {
    (void)ud; Row *r = (Row *)row;
    switch (col) {
        case 0: return r->name;      /* stable heap ptr */
        case 1: return r->age;       /* stable per-row field */
        case 2: return r->score;     /* stable per-row field */
    }
    return "";
}
static Row mk(const char *n, int a, double s) {
    Row r; r.name = strdup(n);
    snprintf(r.age, sizeof r.age, "%d", a);
    snprintf(r.score, sizeof r.score, "%.1f", s);
    return r;
}

int main(void) {
    Row rows[4] = { mk("Zoe",30,9.5), mk("ann",30,8.0), mk("Bob",22,7.5), mk("Ann",25,6.0) };
    void *ptrs[4] = { &rows[0], &rows[1], &rows[2], &rows[3] };
    /* `ptrs` holds the rows; sorting reorders ptrs, rows array is untouched. */

    wubusort_col c0 = {0,0,0};
    CK(wubusort_rows(ptrs,4,&c0,1,cell,NULL)==0,"sort by name");
    /* case-insensitive: "Ann"/"ann" tie (stable -> original order: ann then Ann), then Bob, Zoe */
    CK(((Row*)ptrs[0])->age[0]=='3' && ((Row*)ptrs[0])->age[1]=='0' && ((Row*)ptrs[1])->age[0]=='2' && ((Row*)ptrs[2])->age[0]=='2' && ((Row*)ptrs[3])->age[0]=='3',
       "case-insensitive lexicographic order: ann,Ann,Bob,Zoe");
    CK(strcmp(((Row*)ptrs[0])->name,"ann")==0 && strcmp(((Row*)ptrs[1])->name,"Ann")==0 &&
       strcmp(((Row*)ptrs[2])->name,"Bob")==0 && strcmp(((Row*)ptrs[3])->name,"Zoe")==0, "name order exact");

    /* secondary: same age -> lower score first (score asc numeric) */
    wubusort_col c[2] = { {1,0,1}, {2,0,1} };
    void *p2[4] = { &rows[0],&rows[1],&rows[2],&rows[3] };
    CK(wubusort_rows(p2,4,c,2,cell,NULL)==0,"sort by age then score");
    CK(strcmp(((Row*)p2[0])->name,"Bob")==0 && strcmp(((Row*)p2[1])->name,"Ann")==0,
       "ages 22,25 first");
    /* ages 30 tie: ann(8.0) before Zoe(9.5) */
    CK(strcmp(((Row*)p2[2])->name,"ann")==0 && strcmp(((Row*)p2[3])->name,"Zoe")==0,
       "age30 tie broken by score asc");

    /* numeric desc */
    wubusort_col dc = {1,1,1};
    void *p3[4] = { &rows[0],&rows[1],&rows[2],&rows[3] };
    wubusort_rows(p3,4,&dc,1,cell,NULL);
    CK(strcmp(((Row*)p3[0])->name,"Zoe")==0 && strcmp(((Row*)p3[3])->name,"Bob")==0,
       "age descending (Zoe/ann 30 first, Bob 22 last)");

    /* numcmp */
    CK(wubusort_numcmp("10","9")>0,"numeric 10>9");
    CK(wubusort_numcmp("0.5","0.25")>0,"0.5>0.25");
    CK(wubusort_numcmp("-3","2")<0,"-3<2");
    CK(wubusort_numcmp("007","7")==0,"007==7");

    int i; for(i=0;i<4;i++) free(rows[i].name);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusort (stable multi-col lexicographic+numeric, asc/desc, tie-break)\n");
    return 0;
}
