/* test_focus.c */
#include "focus.h"
#include <stdio.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Focus *f = focus_create();
    CK(focus_enabled(f)==1,"default on");
    CK(focus_width(f)==2,"default width");
    focus_set_color(f, 255,0,0,200);
    focus_set_width(f, 4);
    focus_set_enabled(f, 0);
    CK(focus_color(f)==0xFF0000C8u,"color");
    CK(focus_width(f)==4,"width");
    CK(focus_enabled(f)==0,"disabled");
    focus_destroy(f);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: focus (color/width/enabled)\n"); return 0;
}
