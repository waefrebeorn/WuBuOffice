/* test_dyslexia.c */
#include "dyslexia.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Dyslexia *d = dyslexia_create();
    CK(dyslexia_enabled(d)==0,"default off");
    CK(fabsf(dyslexia_spacing(d)-1.5f)<1e-3,"default spacing");
    dyslexia_set_enabled(d, 1);
    dyslexia_set_face(d, "OpenDyslexic");
    dyslexia_set_spacing(d, 0.5f); /* clamp up to 1.0 */
    CK(dyslexia_enabled(d)==1,"on");
    CK(strcmp(dyslexia_face(d),"OpenDyslexic")==0,"face");
    CK(fabsf(dyslexia_spacing(d)-1.0f)<1e-3,"spacing clamp");
    dyslexia_destroy(d);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: dyslexia (enabled/face/spacing-clamp)\n"); return 0;
}
