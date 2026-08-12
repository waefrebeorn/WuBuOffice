#include "wubusolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    /* 2x + 3y = 8; 4x - y = 2  -> x=1, y=2 */
    double A[4] = {2,3, 4,-1}, b[2] = {8,2}, x[2];
    CK(wubusolver_solve(A,2,b,x)==0,"solve 2x2");
    CK(fabs(x[0]-1.0)<1e-9 && fabs(x[1]-2.0)<1e-9,"x=1 y=2");

    double xs, ys;
    CK(wubusolver_solve2(2,3,4,-1,8,2,&xs,&ys)==0,"solve2 helper");
    CK(fabs(xs-1.0)<1e-9 && fabs(ys-2.0)<1e-9,"solve2 values");

    /* 3x3: x+y+z=6; 2y+5z=-4; 2x+5y-z=27  -> x=5, y=3, z=-2 */
    double A3[9] = {1,1,1, 0,2,5, 2,5,-1}, b3[3] = {6,-4,27}, x3[3];
    CK(wubusolver_solve(A3,3,b3,x3)==0,"solve 3x3");
    CK(fabs(x3[0]-5.0)<1e-8 && fabs(x3[1]-3.0)<1e-8 && fabs(x3[2]+2.0)<1e-8,"3x3 values");

    /* singular matrix */
    double S[4] = {1,2, 2,4}, bs[2] = {1,2};
    CK(wubusolver_solve(S,2,bs,x)==-1,"singular rejected");

    /* determinant of [[2,3],[4,-1]] = 2*(-1) - 4*3 = -14 */
    double det = wubusolver_det(A,2);
    CK(fabs(det - (-14.0)) < 1e-9, "det 2x2 = -14");

    /* inverse of A: A * Ainv = I */
    double inv[4], prod[4];
    CK(wubusolver_inv(A,2,inv)==0,"inv 2x2");
    for (int i=0;i<2;i++) for (int j=0;j<2;j++){ prod[i*2+j]=0; for(int k=0;k<2;k++) prod[i*2+j]+=A[i*2+k]*inv[k*2+j]; }
    CK(fabs(prod[0]-1)<1e-9 && fabs(prod[1])<1e-9 && fabs(prod[2])<1e-9 && fabs(prod[3]-1)<1e-9,"A*Ainv=I");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusolver (Gaussian elimination solve 2x2/3x3, det, inv, singular detection)\n");
    return 0;
}
