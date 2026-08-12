#include "wubupivot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

typedef struct { char *region, *product; int sales; } Row;
static const char *rowf(void *r, void *u){ (void)u; return ((Row*)r)->region; }
static const char *colf(void *r, void *u){ (void)u; return ((Row*)r)->product; }
static const char *valf(void *r, void *u){ (void)u; static char b[32]; snprintf(b,sizeof b,"%d",((Row*)r)->sales); return b; }
static Row mk(const char *rg, const char *pr, int s){ Row r; r.region=strdup(rg); r.product=strdup(pr); r.sales=s; return r; }

int main(void) {
    Row rows[5] = { mk("East","Apples",10), mk("East","Pears",20), mk("West","Apples",5), mk("West","Pears",15), mk("East","Apples",30) };
    void *p[5] = { &rows[0],&rows[1],&rows[2],&rows[3],&rows[4] };

    wubupiv *t = wubupiv_build(p,5,rowf,colf,valf,WUBUPIV_SUM,NULL);
    CK(t!=NULL,"pivot built");
    CK(t->nrows==2 && t->ncols==2,"2x2 pivot");
    double v;
    CK(wubupiv_get(t,"East","Apples",&v)==0 && v==40,"East/Apples sum=40");
    CK(wubupiv_get(t,"East","Pears",&v)==0 && v==20,"East/Pears sum=20");
    CK(wubupiv_get(t,"West","Pears",&v)==0 && v==15,"West/Pears sum=15");
    CK(wubupiv_get(t,"North","Apples",&v)==-1,"absent row key");
    wubupiv_free(t);

    /* COUNT: each cell = number of rows in that slice */
    wubupiv *c = wubupiv_build(p,5,rowf,colf,valf,WUBUPIV_COUNT,NULL);
    CK(c!=NULL,"pivot count built");
    CK(wubupiv_get(c,"East","Apples",&v)==0 && v==2,"East/Apples count=2");
    wubupiv_free(c);

    /* AVG */
    wubupiv *a = wubupiv_build(p,5,rowf,colf,valf,WUBUPIV_AVG,NULL);
    CK(wubupiv_get(a,"East","Apples",&v)==0 && v==20,"East/Apples avg=20");
    wubupiv_free(a);

    int i; for(i=0;i<5;i++){ free(rows[i].region); free(rows[i].product); }
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubupivot (2D cross-tab, SUM/COUNT/AVG, absent-key)\n");
    return 0;
}
