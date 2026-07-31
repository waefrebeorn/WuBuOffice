/* test_imgops.c -- unit tests for image preprocessing ops (imgops.c). */
#include "imgops.h"
#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OcrImage *make_grad(int W,int H){ /* horizontal gradient 0..255 */
    OcrImage *im=ocr_image_create((size_t)W,(size_t)H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++)
        ocr_image_set(im,(size_t)x,(size_t)y,(uint8_t)(255*x/(W>1?W-1:1)));
    return im;
}
static OcrImage *make_uni(int W,int H,uint8_t v){
    OcrImage *im=ocr_image_create((size_t)W,(size_t)H);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) ocr_image_set(im,(size_t)x,(size_t)y,v);
    return im;
}

int main(void){
    int fail=0;
    { /* rotate 0 deg = identity */
        OcrImage *g=make_grad(20,10); OcrImage *r=ocr_image_rotate(g,0,0);
        if(!r){ printf("FAIL rotate null\n"); fail=1; }
        else { int ok=1; for(int y=0;y<10;y++) for(int x=0;x<20;x++)
                   if(ocr_image_get(r,(size_t)x,(size_t)y)!=ocr_image_get(g,(size_t)x,(size_t)y)){ok=0;break;}
               if(!ok){ printf("FAIL rotate0 not identity\n"); fail=1; } }
        ocr_image_free(g); ocr_image_free(r);
    }
    { /* rotate 90 then -90 ~ identity (corners may shift; check center) */
        OcrImage *g=make_grad(30,30); OcrImage *r1=ocr_image_rotate(g,90,0);
        OcrImage *r2=ocr_image_rotate(r1,-90,0);
        int ok=1; for(int y=5;y<25;y++) for(int x=5;x<25;x++)
            if(ocr_image_get(r2,(size_t)x,(size_t)y)!=ocr_image_get(g,(size_t)x,(size_t)y)){ok=0;break;}
        if(!r2||!ok){ printf("FAIL rotate90 roundtrip\n"); fail=1; }
        ocr_image_free(g);ocr_image_free(r1);ocr_image_free(r2);
    }
    { /* contrast stretch of a flat image is still flat */
        OcrImage *u=make_uni(16,16,100); OcrImage *c=ocr_image_contrast_stretch(u,2,98);
        int mn=255,mx=0; for(int y=0;y<16;y++) for(int x=0;x<16;x++){int v=ocr_image_get(c,(size_t)x,(size_t)y); if(v<mn)mn=v; if(v>mx)mx=v;}
        if(mx-mn>1){ printf("FAIL contrast flat not flat (mn=%d mx=%d)\n",mn,mx); fail=1; }
        ocr_image_free(u);ocr_image_free(c);
    }
    { /* contrast stretch expands a narrow-range image toward full 0..255 */
        OcrImage *u=make_grad(16,16); /* 0..255 already full range -> no-op, use scaled */
        /* build a narrow-range image [100,120] */
        OcrImage *n=ocr_image_create(16,16);
        for(int y=0;y<16;y++) for(int x=0;x<16;x++)
            ocr_image_set(n,(size_t)x,(size_t)y,(uint8_t)(100 + (x*20/15)));
        OcrImage *c=ocr_image_contrast_stretch(n,2,98);
        int mn=255,mx=0; for(int y=0;y<16;y++) for(int x=0;x<16;x++){int v=ocr_image_get(c,(size_t)x,(size_t)y); if(v<mn)mn=v; if(v>mx)mx=v;}
        if(mx-mn<=20){ printf("FAIL contrast did not expand (mn=%d mx=%d)\n",mn,mx); fail=1; }
        ocr_image_free(u); ocr_image_free(n); ocr_image_free(c);
    }
    { /* median removes an isolated salt speck */
        OcrImage *u=make_uni(11,11,10);
        ocr_image_set(u,5,5,255); /* isolated bright pixel */
        OcrImage *m=ocr_image_median(u,1);
        if(ocr_image_get(m,5,5)==255){ printf("FAIL median kept speck\n"); fail=1; }
        if(ocr_image_get(m,0,0)!=10){ printf("FAIL median corrupted flat\n"); fail=1; }
        ocr_image_free(u);ocr_image_free(m);
    }
    { /* shading correction does not crash / returns same size, nonzero */
        OcrImage *g=make_grad(40,40); OcrImage *s=ocr_image_shading_correct(g,8);
        if(!s){ printf("FAIL shading null\n"); fail=1; }
        else { long sum=0; for(int y=0;y<40;y++) for(int x=0;x<40;x++) sum+=ocr_image_get(s,(size_t)x,(size_t)y);
               if(sum<=0){ printf("FAIL shading all zero\n"); fail=1; } }
        ocr_image_free(g);ocr_image_free(s);
    }
    { /* sharpen of a flat image stays flat */
        OcrImage *u=make_uni(16,16,128); OcrImage *s=ocr_image_sharpen(u,1,1.0);
        int ok=1; for(int y=0;y<16;y++) for(int x=0;x<16;x++) if(ocr_image_get(s,(size_t)x,(size_t)y)!=128){ok=0;break;}
        if(!ok){ printf("FAIL sharpen changed flat\n"); fail=1; }
        ocr_image_free(u);ocr_image_free(s);
    }
    if(fail){ printf("FAIL test_imgops\n"); return 1; }
    printf("PASS test_imgops\n");
    return 0;
}
