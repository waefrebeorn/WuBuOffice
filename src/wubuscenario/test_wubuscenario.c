#include "wubuscenario.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

static int applied[4][4];
static int apply_cb(int row, int col, const char *v, void *u) {
    (void)u; applied[row][col] = atoi(v); return 0;
}

int main(void) {
    wubuscenario *s = wubuscenario_create();
    CK(s != NULL, "create");

    wubuscen_cell c1[2] = { {0,0,"10"}, {1,1,"20"} };
    CK(wubuscenario_set(s,"Pessimistic",c1,2)==0,"set pessim");
    wubuscen_cell c2[1] = { {0,0,"50"} };
    CK(wubuscenario_set(s,"Optimistic",c2,1)==0,"set optim");
    CK(wubuscenario_count(s)==2,"2 scenarios");

    const wubuscen_entry *e = wubuscenario_get(s,"Pessimistic");
    CK(e && e->n==2 && strcmp(e->name,"Pessimistic")==0,"get pessim");
    CK(e->cells[0].row==0 && e->cells[0].col==0 && strcmp(e->cells[0].value,"10")==0,"cell0");
    CK(wubuscenario_get(s,"Nope")==NULL,"absent");

    CK(wubuscenario_apply(s,"Pessimistic",apply_cb,NULL)==0,"apply");
    CK(applied[0][0]==10 && applied[1][1]==20,"applied values");
    CK(applied[0][1]==0,"other cell untouched");

    CK(strcmp(wubuscenario_name(s,0),"Pessimistic")==0,"enum name0");

    wubuscenario_destroy(s);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuscenario (named what-if scenarios, apply/restore, enum)\n");
    return 0;
}
