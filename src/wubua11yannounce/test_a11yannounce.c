/* test_a11yannounce.c */
#include "a11yannounce.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    A11yAnnounce *a = a11y_announce_create();
    CK(a11y_announce_pending(a)==0,"empty");
    a11y_announce_push(a, "Inserted paragraph");
    a11y_announce_push(a, "Deleted table");
    CK(a11y_announce_pending(a)==2,"pending");
    char *m1 = a11y_announce_pop(a);
    CK(m1 && strcmp(m1,"Inserted paragraph")==0,"pop order");
    free(m1);
    char *m2 = a11y_announce_pop(a);
    CK(m2 && strcmp(m2,"Deleted table")==0,"pop2");
    free(m2);
    CK(a11y_announce_pending(a)==0,"drained");
    CK(a11y_announce_pop(a)==NULL,"null when empty");
    a11y_announce_destroy(a);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: a11yannounce (FIFO queue)\n"); return 0;
}
