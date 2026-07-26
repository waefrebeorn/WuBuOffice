/* imgops.c -- see imgops.h. Clean-room C11, O(area) per op. */
#include "imgops.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static OcrImage *clone_like(const OcrImage *im){
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    return ocr_image_create((size_t)W,(size_t)H);
}
static double deg2rad(double d){ return d*3.14159265358979323846/180.0; }

OcrImage *ocr_image_rotate(const OcrImage *im, double deg, uint8_t fill){
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *o=clone_like(im); if(!o) return NULL;
    double a=deg2rad(deg);
    double ca=cos(a), sa=sin(a);
    double cx=(W-1)/2.0, cy=(H-1)/2.0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        double dx=x-cx, dy=y-cy;
        double sx=cx + dx*ca + dy*sa;
        double sy=cy - dx*sa + dy*ca;
        int ix=(int)floor(sx+0.5), iy=(int)floor(sy+0.5);
        uint8_t v = (ix>=0&&ix<W&&iy>=0&&iy<H) ? ocr_image_get(im,(size_t)ix,(size_t)iy) : fill;
        ocr_image_set(o,(size_t)x,(size_t)y,v);
    }
    return o;
}

OcrImage *ocr_image_contrast_stretch(const OcrImage *im, int lo_pct, int hi_pct){
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *o=clone_like(im); if(!o) return NULL;
    if(lo_pct<0)lo_pct=0; if(hi_pct>100)hi_pct=100; if(lo_pct>=hi_pct)hi_pct=lo_pct+1;
    size_t hist[256]; memset(hist,0,sizeof hist);
    size_t total=(size_t)W*H;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) hist[ocr_image_get(im,(size_t)x,(size_t)y)]++;
    size_t lo_n=total*lo_pct/100, hi_n=total*hi_pct/100;
    int lo=0, hi=255; size_t acc=0;
    for(int i=0;i<256;i++){ acc+=hist[i]; if(acc>=lo_n){lo=i;break;} }
    acc=0; for(int i=0;i<256;i++){ acc+=hist[i]; if(acc>=hi_n){hi=i;break;} }
    if(hi<=lo) hi=lo+1;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int v=ocr_image_get(im,(size_t)x,(size_t)y);
        int s=(v-lo)*255/(hi-lo); if(s<0)s=0; if(s>255)s=255;
        ocr_image_set(o,(size_t)x,(size_t)y,(uint8_t)s);
    }
    return o;
}

OcrImage *ocr_image_median(const OcrImage *im, int radius){
    if(radius<1)radius=1;
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *o=clone_like(im); if(!o) return NULL;
    int win=2*radius+1; int n=win*win;
    uint8_t *buf=malloc((size_t)n); if(!buf){ocr_image_free(o);return NULL;}
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int k=0;
        for(int dy=-radius;dy<=radius;dy++) for(int dx=-radius;dx<=radius;dx++){
            int xx=x+dx, yy=y+dy;
            uint8_t v=(xx>=0&&xx<W&&yy>=0&&yy<H)?ocr_image_get(im,(size_t)xx,(size_t)yy):ocr_image_get(im,(size_t)x,(size_t)y);
            buf[k++]=v;
        }
        /* inplace insertion sort (small n) */
        for(int i=1;i<n;i++){ uint8_t t=buf[i]; int j=i-1; while(j>=0&&buf[j]>t){buf[j+1]=buf[j];j--;} buf[j+1]=t; }
        ocr_image_set(o,(size_t)x,(size_t)y,buf[n/2]);
    }
    free(buf);
    return o;
}

/* box-mean blur (used by shading + sharpen). separable. */
static void box_blur(const OcrImage *im, OcrImage *o, int r){
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *tmp=clone_like(im); if(!tmp) return;
    int win=2*r+1;
    for(int y=0;y<H;y++){
        long s=0; int cnt=0;
        for(int x=-r;x<=r;x++){ int xx=x<0?0:(x>=W?W-1:x); s+=ocr_image_get(im,(size_t)xx,(size_t)y); cnt++; }
        for(int x=0;x<W;x++){
            ocr_image_set(tmp,(size_t)x,(size_t)y,(uint8_t)(s/cnt));
            int add=x+r+1, sub=x-r;
            int xa=add>=W?W-1:add, xs=sub<0?0:sub;
            s+=ocr_image_get(im,(size_t)xa,(size_t)y)-ocr_image_get(im,(size_t)xs,(size_t)y);
        }
    }
    for(int x=0;x<W;x++){
        long s=0; int cnt=0;
        for(int y=-r;y<=r;y++){ int yy=y<0?0:(y>=H?H-1:y); s+=ocr_image_get(tmp,(size_t)x,(size_t)yy); cnt++; }
        for(int y=0;y<H;y++){
            ocr_image_set(o,(size_t)x,(size_t)y,(uint8_t)(s/cnt));
            int add=y+r+1, sub=y-r;
            int ya=add>=H?H-1:add, ys=sub<0?0:sub;
            s+=ocr_image_get(tmp,(size_t)x,(size_t)ya)-ocr_image_get(tmp,(size_t)x,(size_t)ys);
        }
    }
    ocr_image_free(tmp);
}

OcrImage *ocr_image_shading_correct(const OcrImage *im, int blur_radius){
    if(blur_radius<2)blur_radius=2;
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *field=clone_like(im); if(!field) return NULL;
    box_blur(im,field,blur_radius);
    OcrImage *o=clone_like(im); if(!o){ocr_image_free(field);return NULL;}
    long fsum=0; for(int y=0;y<H;y++) for(int x=0;x<W;x++) fsum+=ocr_image_get(field,(size_t)x,(size_t)y);
    double fmean=fsum/(double)((size_t)W*H);
    if(fmean<1) fmean=1;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int v=ocr_image_get(im,(size_t)x,(size_t)y);
        int f=ocr_image_get(field,(size_t)x,(size_t)y); if(f<1)f=1;
        double corr=(double)v * 128.0 / f;          /* normalize to field mean ~128 */
        int s=(int)(corr* (fmean/128.0));         /* keep overall brightness */
        if(s<0)s=0; if(s>255)s=255;
        ocr_image_set(o,(size_t)x,(size_t)y,(uint8_t)s);
    }
    ocr_image_free(field);
    return o;
}

OcrImage *ocr_image_sharpen(const OcrImage *im, int blur_radius, double amount){
    if(blur_radius<1)blur_radius=1;
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    OcrImage *blur=clone_like(im); if(!blur) return NULL;
    box_blur(im,blur,blur_radius);
    OcrImage *o=clone_like(im); if(!o){ocr_image_free(blur);return NULL;}
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int v=ocr_image_get(im,(size_t)x,(size_t)y);
        int b=ocr_image_get(blur,(size_t)x,(size_t)y);
        int s=(int)(v + amount*(v-b));
        if(s<0)s=0; if(s>255)s=255;
        ocr_image_set(o,(size_t)x,(size_t)y,(uint8_t)s);
    }
    ocr_image_free(blur);
    return o;
}
