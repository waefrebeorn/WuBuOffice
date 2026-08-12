#include "wubugoalseek.h"
#include <math.h>
#include <stdlib.h>

int wubugoalseek(wubugoalseek_fn f, double target, double lo, double hi,
                 double tol, double eps, int scan, void *ud, wubugoalseek_result *r) {
    if (!f || !r || hi <= lo) return -1;
    if (tol <= 0) tol = 1e-9;
    if (eps <= 0) eps = 1e-9;
    if (scan < 1) scan = 32;
    r->converged = 0;

    double flo = f(lo, ud) - target, fhi = f(hi, ud) - target;
    /* if not bracketing, scan for a bracket */
    if (flo * fhi > 0) {
        int br = 0;
        double a = lo;
        for (int i = 1; i <= scan && !br; i++) {
            double b = lo + (hi - lo) * (double)i / (double)scan;
            double fa = f(a, ud) - target, fb = f(b, ud) - target;
            if (fa == 0) { r->x = a; r->f_of_x = target; r->converged = 1; return 0; }
            if (fa * fb < 0) { lo = a; hi = b; flo = fa; fhi = fb; br = 1; }
            a = b;
        }
        if (!br) {
            /* pick nearest endpoint */
            double da = fabs(flo), db = fabs(fhi);
            r->x = (da < db) ? lo : hi;
            r->f_of_x = f(r->x, ud);
            r->iters = scan;
            return 0;
        }
    } else if (flo == 0) {
        r->x = lo; r->f_of_x = target; r->iters = 0; r->converged = 1; return 0;
    }

    double a = lo, b = hi, fa = flo, fb = fhi, c = a, fc = fa;
    int iters = 0;
    for (iters = 0; iters < 200; iters++) {
        if (fabs(b - a) < eps) { r->x = (a + b) / 2; r->f_of_x = f(r->x, ud); r->iters = iters; r->converged = 1; return 0; }
        /* secant step */
        double s = b - fb * (b - a) / (fb - fa);
        double fs = f(s, ud) - target;
        if (fabs(fs) <= tol) { r->x = s; r->f_of_x = fs + target; r->iters = iters; r->converged = 1; return 0; }
        /* bisection step to keep bracket */
        double m = (a + b) / 2, fm = f(m, ud) - target;
        if (fabs(fm) <= tol) { r->x = m; r->f_of_x = fm + target; r->iters = iters; r->converged = 1; return 0; }
        if (fa * fm < 0) { b = m; fb = fm; } else { a = m; fa = fm; }
        c = s; fc = fs;
    }
    r->x = (a + b) / 2; r->f_of_x = f(r->x, ud); r->iters = iters;
    return 0;
}

int wubugoalseek_fit(const double *x, const double *y, int n,
                     double *slope, double *intercept, double *r2) {
    if (!x || !y || n < 2 || !slope || !intercept) return -1;
    double sx=0, sy=0, sxx=0, sxy=0, syy=0;
    for (int i = 0; i < n; i++) { sx+=x[i]; sy+=y[i]; sxx+=x[i]*x[i]; sxy+=x[i]*y[i]; syy+=y[i]*y[i]; }
    double den = (double)n * sxx - sx * sx;
    if (den == 0) return -1;
    *slope = ((double)n * sxy - sx * sy) / den;
    *intercept = (sy - *slope * sx) / (double)n;
    if (r2) {
        double ss_tot = syy - sy * sy / (double)n;
        double ss_res = 0;
        for (int i = 0; i < n; i++) {
            double e = y[i] - (*slope * x[i] + *intercept);
            ss_res += e * e;
        }
        *r2 = (ss_tot == 0) ? 1.0 : 1.0 - ss_res / ss_tot;
    }
    return 0;
}
