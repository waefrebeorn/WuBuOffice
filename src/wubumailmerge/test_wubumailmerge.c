#include "wubumailmerge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubumailmerge *m = wubumailmerge_create();
    const wubumailmerge_field rec1[] = {{"Name","Alice"},{"City","Boston"},{"Amount","100"},{NULL,NULL}};
    const wubumailmerge_field rec2[] = {{"Name","Bob"},{"City","Denver"},{"Amount","250"},{NULL,NULL}};
    CK(wubumailmerge_add_record(m,rec1)==0 && wubumailmerge_add_record(m,rec2)==0,"add 2 records");
    CK(wubumailmerge_record_count(m)==2,"count 2");

    const char *tpl = "Dear ${Name}, welcome to {City}. Total: ${Amount}.";
    char *o1 = wubumailmerge_merge(m,0,tpl);
    char *o2 = wubumailmerge_merge(m,1,tpl);
    CK(o1 && strcmp(o1,"Dear Alice, welcome to Boston. Total: 100.")==0,"merge rec0");
    CK(o2 && strcmp(o2,"Dear Bob, welcome to Denver. Total: 250.")==0,"merge rec1");

    /* unknown field left as-is */
    const char *tpl2 = "Hi ${Name}, ref ${Missing}.";
    char *o3 = wubumailmerge_merge(m,0,tpl2);
    CK(o3 && strcmp(o3,"Hi Alice, ref ${Missing}.")==0,"unknown field passthrough");

    free(o1); free(o2); free(o3);
    wubumailmerge_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumailmerge (template ${field}/{field} fill from records, unknown passthrough)\n");
    return 0;
}
