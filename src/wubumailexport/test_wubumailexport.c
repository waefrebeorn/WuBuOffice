#include "wubumailexport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubumailexport m = {0};
    CK(wubumailexport_build(&m,"bob@example.com","alice@example.com","Report","Hello Bob\nHere is the doc.")==0, "build");
    CK(strcmp(m.subject,"Report")==0, "subject");
    CK(m.bodylen==26, "body len");

    char *msg = wubumailexport_render(&m);
    CK(msg != NULL, "render");
    if (msg){
        CK(strstr(msg,"To: bob@example.com")!=NULL, "to header");
        CK(strstr(msg,"From: alice@example.com")!=NULL, "from header");
        CK(strstr(msg,"Subject: Report")!=NULL, "subject header");
        CK(strstr(msg,"\r\n\r\nHello Bob")!=NULL, "blank-line separator");
    }
    free(msg);
    wubumailexport_free(&m);
    CK(m.body == NULL, "free clears body");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumailexport (RFC-5322 mail message render)\n");
    return 0;
}
