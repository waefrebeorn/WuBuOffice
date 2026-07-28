/* test_sandbox.c */
#include "sandbox.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Sandbox *s = sandbox_create();
    int id = sandbox_register(s, "spellcheck", SBX_READ_DOC|SBX_WRITE_DOC|SBX_NET);
    CK(id>=0,"register");
    /* deny-by-default: nothing granted yet */
    CK(sandbox_check(s,id,SBX_READ_DOC)==0,"deny by default");
    CK(sandbox_denials(s,id)==1,"denial counted");
    /* grant read+write but NOT net */
    CK(sandbox_grant(s,id,SBX_READ_DOC|SBX_WRITE_DOC)==1,"grant");
    CK(sandbox_check(s,id,SBX_READ_DOC)==1,"read allowed");
    CK(sandbox_check(s,id,SBX_WRITE_DOC)==1,"write allowed");
    CK(sandbox_check(s,id,SBX_NET)==0,"net denied");
    /* granting a cap the plugin never requested has no effect */
    sandbox_grant(s,id,SBX_FS|SBX_READ_DOC);
    CK(sandbox_check(s,id,SBX_FS)==0,"unrequested cap still denied");
    CK(sandbox_effective(s,id)==SBX_READ_DOC,"effective = requested&granted");
    CK(strcmp(sandbox_name(s,id),"spellcheck")==0,"name");
    CK(sandbox_check(s,99,SBX_READ_DOC)==0,"bad id denied");
    sandbox_destroy(s);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: sandbox (deny-by-default capability gates)\n"); return 0;
}
