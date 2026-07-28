/* test_form.c */
#include "form.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Form *f = form_create();
    CK(form_add(f,"name",FORM_TEXT,"")==1,"add text");
    CK(form_add(f,"agree",FORM_CHECKBOX,"off")==1,"add checkbox");
    CK(form_add(f,"name",FORM_TEXT,"dup")==0,"dup rejected");
    CK(form_set_value(f,"name","Alice")==1,"set");
    CK(strcmp(form_value(f,"name"),"Alice")==0,"get value");
    CK(form_type(f,"agree")==FORM_CHECKBOX,"type");
    CK(form_value(f,"missing")==NULL,"missing null");
    CK(form_count(f)==2,"count");
    CK(strcmp(form_name_at(f,0),"name")==0,"name at");
    form_destroy(f);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: form (add/set/type/lookup)\n"); return 0;
}
