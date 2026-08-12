/* wubugoalseek.h — numeric root-finding / goal-seek (find x s.t. f(x)=target). */
#ifndef WUBUGOALSEEK_H
#define WUBUGOALSEEK_H

/* f(x, ud) -> value. Goal-seek finds x in [lo,hi] where f(x) == target. */
typedef double (*wubugoalseek_fn)(double x, void *ud);

typedef struct {
    double x;        /* solution */
    double f_of_x;   /* f(x) at solution */
    int iters;
    int converged;   /* 1 if |f-target| within tol or bisection narrowed to eps */
} wubugoalseek_result;

/* Bisection + secant hybrid. Searches [lo, hi]; if f(lo) and f(hi) don't
 * bracket target, it scans up to `scan` subintervals for a bracket. Returns 0
 * and fills `r`; returns -1 on invalid args (lo==hi, NULL fn). */
int wubugoalseek(wubugoalseek_fn f, double target, double lo, double hi,
                 double tol, double eps, int scan, void *ud, wubugoalseek_result *r);

/* Least-squares linear fit of (x[],y[]) -> slope, intercept, r^2. */
int wubugoalseek_fit(const double *x, const double *y, int n,
                     double *slope, double *intercept, double *r2);

#endif
