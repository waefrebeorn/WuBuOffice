/* lens.c -- Office-Lens-style document flattening (see lens.h). */
#include "lens.h"
#include "image.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* small linear solve: solve A*x = b for n<=8, A row-major n*n, b length n.
 * Returns 0 on success, -1 if singular. */
static int solve_linear(int n, double *A, double *b, double *x){
    /* Gaussian elimination with partial pivoting */
    int *piv = malloc(sizeof(int)*n);
    for(int i=0;i<n;i++) piv[i]=i;
    for(int col=0;col<n;col++){
        int best=col; double bestv=fabs(A[col*n+col]);
        for(int r=col+1;r<n;r++){ double v=fabs(A[r*n+col]); if(v>bestv){bestv=v;best=r;} }
        if(bestv<1e-12){ free(piv); return -1; }
        if(best!=col){ for(int k=0;k<n;k++){double t=A[best*n+k];A[best*n+k]=A[col*n+k];A[col*n+k]=t;} double tb=b[best];b[best]=b[col];b[col]=tb; }
        for(int r=col+1;r<n;r++){
            double f=A[r*n+col]/A[col*n+col];
            for(int k=col;k<n;k++) A[r*n+k]-=f*A[col*n+k];
            b[r]-=f*b[col];
        }
    }
    for(int i=n-1;i>=0;i--){ double s=b[i]; for(int k=i+1;k<n;k++) s-=A[i*n+k]*x[k]; x[i]=s/A[i*n+i]; }
    free(piv);
    return 0;
}

/* bilinear sample of grayscale src at (x,y) using the opaque API. */
static uint8_t sample(const OcrImage *s, double x, double y){
    double fx=floor(x), fy=floor(y);
    int x0=(int)fx, y0=(int)fy;
    double dx=x-fx, dy=y-fy;
    int x1=x0+1, y1=y0+1;
    int W=(int)ocr_image_width(s), H=(int)ocr_image_height(s);
    double v00 = (x0>=0&&y0>=0&&x0<W&&y0<H)? ocr_image_get(s,x0,y0):255.0;
    double v10 = (x1>=0&&y0>=0&&x1<W&&y0<H)? ocr_image_get(s,x1,y0):255.0;
    double v01 = (x0>=0&&y1>=0&&x0<W&&y1<H)? ocr_image_get(s,x0,y1):255.0;
    double v11 = (x1>=0&&y1>=0&&x1<W&&y1<H)? ocr_image_get(s,x1,y1):255.0;
    double top=v00+(v10-v00)*dx, bot=v01+(v11-v01)*dx;
    double v=top+(bot-top)*dy;
    return (uint8_t)(v<0?0:(v>255?255:v));
}

/* average brightness of all 4 corners -> choose output ordering.
 * We sort by (y then x): top two = smaller y, bottom two = larger y;
 * within top, left=smaller x; within bottom, left=smaller x. */
static void order_corners(Pt2 in[4], Pt2 out[4]){
    Pt2 a[4]; memcpy(a,in,sizeof(Pt2)*4);
    /* bubble-ish sort by y then x */
    for(int i=0;i<4;i++) for(int j=i+1;j<4;j++){
        double ya=a[i].y + (a[i].y==a[j].y? a[i].x*1e-6 : 0);
        double yb=a[j].y + (a[i].y==a[j].y? a[j].x*1e-6 : 0);
        if(ya>yb){ Pt2 t=a[i]; a[i]=a[j]; a[j]=t; }
    }
    /* now a[0],a[1] are top-ish, a[2],a[3] bottom-ish; order left/right */
    if(a[0].x > a[1].x){ Pt2 t=a[0]; a[0]=a[1]; a[1]=t; }
    if(a[2].x > a[3].x){ Pt2 t=a[2]; a[2]=a[3]; a[3]=t; }
    out[0]=a[0]; out[1]=a[1]; out[2]=a[3]; out[3]=a[2]; /* TL,TR,BR,BL */
}

OcrImage *lens_flatten(const OcrImage *src, Pt2 corners[4], int out_w, int out_h, int contrast){
    Pt2 q[4]; order_corners(corners,q);
    /* destination: unit square corners (0,0)(W,0)(W,H)(0,H) mapped to q.
       Solve homography H (3x3) such that q_i = H * dst_i (in homogeneous). */
    double Wd = hypot(q[1].x-q[0].x, q[1].y-q[0].y);
    double Hd = hypot(q[3].x-q[0].x, q[3].y-q[0].y);
    int OW = out_w>0? out_w : (int)(Wd>0? Wd : 100);
    int OH = out_h>0? out_h : (int)(Hd>0? Hd : 100);
    if(OW<2||OH<2) return NULL;

    /* Build 8x8 system: for each of 4 dst corners (0,0),(OW,0),(OW,OH),(0,OH):
         x' = (a*x + b*y + c)/(g*x + h*y + 1)
         y' = (d*x + e*y + f)/(g*x + h*y + 1)
       unknowns a..h. 2 eq per corner. */
    double A[64], b[8]; memset(A,0,sizeof A);
    Pt2 dst[4] = {{0,0},{OW,0},{OW,OH},{0,OH}};
    for(int i=0;i<4;i++){
        double X=dst[i].x, Y=dst[i].y, Xp=q[i].x, Yp=q[i].y;
        int r=2*i;
        /* x' eq: Xp*(g*X+h*Y+1) = a*X+b*Y+c -> aX+bY+c - g*X*Xp - h*Y*Xp = Xp */
        A[r*8+0]=X; A[r*8+1]=Y; A[r*8+2]=1; A[r*8+6]=-X*Xp; A[r*8+7]=-Y*Xp; b[r]=Xp;
        /* y' eq: Yp*(g*X+h*Y+1) = d*X+e*Y+f -> dX+eY+f - g*X*Yp - h*Y*Yp = Yp */
        A[(r+1)*8+3]=X; A[(r+1)*8+4]=Y; A[(r+1)*8+5]=1; A[(r+1)*8+6]=-X*Yp; A[(r+1)*8+7]=-Y*Yp; b[r+1]=Yp;
    }
    double hcoef[8];
    if(solve_linear(8,A,b,hcoef)!=0) return NULL;
    double a=hcoef[0],b1=hcoef[1],c=hcoef[2],d=hcoef[3],e=hcoef[4],f=hcoef[5],g=hcoef[6],hh=hcoef[7];

    OcrImage *out = ocr_image_create(OW,OH);
    if(!out) return NULL;
    for(int y=0;y<OH;y++) for(int x=0;x<OW;x++){
        double denom = g*x + hh*y + 1.0;
        if(fabs(denom)<1e-9){ ocr_image_set(out,x,y,255); continue; }
        double sx = (a*x + b1*y + c)/denom;
        double sy = (d*x + e*y + f)/denom;
        ocr_image_set(out,x,y, sample(src, sx, sy));
    }
    if(contrast){
        /* Otsu-free simple stretch: find min/max */
        int mn=255,mx=0; const uint8_t*p=ocr_image_pixels(out); size_t n=(size_t)OW*OH;
        for(size_t i=0;i<n;i++){ if(p[i]<mn)mn=p[i]; if(p[i]>mx)mx=p[i]; }
        if(mx>mn){
            double sc=255.0/(mx-mn);
            uint8_t *q=(uint8_t*)ocr_image_pixels(out);
            for(size_t i=0;i<n;i++) q[i]=(uint8_t)((q[i]-mn)*sc);
        }
    }
    return out;
}
