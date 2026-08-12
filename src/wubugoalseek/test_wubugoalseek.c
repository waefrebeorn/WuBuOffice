#include "wubugoalseek.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

/* f(x) = x^2 - 2 -> root sqrt(2) ~1.4142 */
static double sq(void *ud, double x) { (void)ud; return x * x - 2.0; }

/* solve 2x + 3 = 11 -> x = 4 */
static double lin(void *ud, double x) { (void)ud; return 2.0 * x + 3.0; }

int main(void) {
    wubugoalseek_result r;
    CK(wubugoalseek(sq, 0.0, 0.0, 2.0, 1e-9, 1e-9, 32, NULL, &r) == 0, "goalseek call");
    CK(fabs(r.x - sqrt(2.0)) < 1e-6, "sqrt2 root");
    CK(r.converged == 1, "converged flag");

    CK(wubugoalseek(lin, 11.0, -10.0, 10.0, 1e-9, 1e-9, 32, NULL, &r) == 0, "linear call");
    CK(fabs(r.x - 4.0) < 1e-6, "linear root x=4");

    /* unbracketed: f(x)=x^2 never reaches -5 in reals; should not crash, returns endpoint-ish */
    wubugoalseek_result nr;
    CK(wubugoalseek(sq, -5.0, 0.0, 2.0, 1e-9, 1e-9, 8, NULL, &nr) == 0, "unbracketed safe");

    /* least squares fit: y = 2x + 1, perfect */
    double x[4] = {1,2,3,4}, y[4] = {3,5,7,9}, s,i,rr;
    CK(wubugoalseek_fit(x,y,4,&s,&i,&rr)==0,"fit call");
    CK(fabs(s-2.0)<1e-9 && fabs(i-1.0)<1e-9,"fit slope=2 intercept=1");
    CK(fabs(rr-1.0)<1e-9,"fit r2=1");

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubugoalseek (root-find sqrt2/linear, unbracketed safe, LSQ fit)\n");
    return 0;
}
