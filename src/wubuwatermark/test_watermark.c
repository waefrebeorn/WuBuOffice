/* test_watermark.c */
#include "watermark.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Watermark *w = watermark_create();
    CK(watermark_enabled(w)==0,"default off");
    watermark_set_text(w,"CONFIDENTIAL");
    watermark_set_angle(w, 315);
    watermark_set_opacity(w, 1.9f); /* clamp */
    watermark_set_enabled(w, 1);
    CK(strcmp(watermark_text(w),"CONFIDENTIAL")==0,"text");
    CK(watermark_angle(w)==315,"angle");
    CK(fabsf(watermark_opacity(w)-1.0f)<1e-3,"opacity clamp");
    CK(watermark_enabled(w)==1,"enabled");
    watermark_destroy(w);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: watermark (text/angle/opacity-clamp/enabled)\n"); return 0;
}
