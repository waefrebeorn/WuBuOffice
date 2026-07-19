/* dft.c -- 2D DFT compression + spectral analysis (see dft.h).
 * Plain scalar C11, no deps. Direct 2D DFT (small glyph crops -> O(N^2) per
 * output coefficient, fine for N<=48). Magnitude-sorted compression for cheap
 * storage, spectral features for warp/style-robust recognition. */
#include "dft.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double mag2(double re, double im) { return re*re + im*im; }

void dft2d(const uint8_t *px, int W, int H, double *re, double *im) {
    /* double precision for stability on small crops */
    for (int v = 0; v < H; v++) {
        for (int u = 0; u < W; u++) {
            double sre = 0.0, sim = 0.0;
            double ang_u = -2.0 * M_PI * (double)u / (double)W;
            double ang_v = -2.0 * M_PI * (double)v / (double)H;
            for (int y = 0; y < H; y++) {
                double phase_y = ang_v * (double)y;
                double cy = cos(phase_y), sy = sin(phase_y);
                for (int x = 0; x < W; x++) {
                    double ph = phase_y + ang_u * (double)x;
                    /* px centered so DC isn't dominated by mean brightness */
                    double val = (double)px[y*W + x] - 128.0;
                    sre += val * cos(ph);
                    sim += val * sin(ph);
                }
                (void)cy; (void)sy;
            }
            re[v*W + u] = sre;
            im[v*W + u] = sim;
        }
    }
}

void idft2d(const double *re, const double *im, int W, int H, uint8_t *out) {
    double inv = 1.0 / ((double)W * (double)H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            double sre = 0.0;
            double ang_u = 2.0 * M_PI * (double)x / (double)W;
            double ang_v = 2.0 * M_PI * (double)y / (double)H;
            for (int v = 0; v < H; v++) {
                double phase_y = ang_v * (double)v;
                for (int u = 0; u < W; u++) {
                    double ph = phase_y + ang_u * (double)u;
                    double cr = cos(ph), sr = sin(ph);
                    sre += re[v*W + u] * cr - im[v*W + u] * sr;
                }
            }
            double val = sre * inv + 128.0;
            if (val < 0.0) val = 0.0;
            if (val > 255.0) val = 255.0;
            out[y*W + x] = (uint8_t)(val + 0.5);
        }
    }
}

typedef struct { int u, v; double mag; double re, im; } Coeff;

/* simple insertion sort by descending magnitude (small K) */
static void sort_desc(Coeff *c, int n) {
    for (int i = 1; i < n; i++) {
        Coeff key = c[i];
        int j = i - 1;
        while (j >= 0 && c[j].mag < key.mag) { c[j+1] = c[j]; j--; }
        c[j+1] = key;
    }
}

int dft_compress(const double *re, const double *im, int W, int H,
                 int keep, double *buf) {
    int N = W * H;
    if (keep > N) keep = N;
    if (keep <= 0) return 0;
    Coeff *c = (Coeff*)malloc(sizeof(Coeff) * N);
    if (!c) return 0;
    for (int i = 0; i < N; i++) {
        c[i].u = i % W; c[i].v = i / W;
        c[i].re = re[i]; c[i].im = im[i];
        c[i].mag = mag2(re[i], im[i]);
    }
    sort_desc(c, N);
    int written = 0;
    for (int i = 0; i < keep; i++) {
        buf[written++] = (double)c[i].u;
        buf[written++] = (double)c[i].v;
        buf[written++] = c[i].re;
        buf[written++] = c[i].im;
    }
    free(c);
    return keep;
}

int dft_features(const double *re, const double *im, int W, int H, double *out) {
    int N = W * H;
    double tot = 0.0, low = 0.0, mid = 0.0, high = 0.0;
    double dom_num_r = 0.0, dom_num_a = 0.0;
    int maxu = 0, maxv = 0; double maxmag = -1.0;
    int half = (W > 1 || H > 1) ? 1 : 0;
    /* skip DC (u=v=0) for band fractions; include it in total energy */
    for (int i = 0; i < N; i++) {
        double m = mag2(re[i], im[i]);
        tot += m;
        int u = i % W, v = i / W;
        if (u == 0 && v == 0) continue;
        double r = sqrt((double)(u*u + v*v));
        double rf = r / (double)(W + H); /* normalized radius */
        if (rf < 0.18) low += m;
        else if (rf < 0.5) mid += m;
        else high += m;
        dom_num_r += r * m;
        double a = atan2((double)v, (double)u);
        dom_num_a += cos(a) * m; /* circular mean x; y below via second pass */
        if (m > maxmag) { maxmag = m; maxu = u; maxv = v; }
    }
    double dc = mag2(re[0], im[0]);
    double ac = tot - dc;                 /* AC energy (exclude DC) */
    double acden = ac > 1e-12 ? ac : 1.0;
    out[0] = tot;
    out[1] = low / acden;                /* band fractions over AC energy */
    out[2] = mid / acden;
    out[3] = high / acden;
    out[4] = (ac > 1e-12) ? dom_num_r / ac : 0.0;
    /* dominant angle from the 2nd-largest peak (true texture direction) */
    double best = -1.0; int bu = 0, bv = 0;
    for (int i = 0; i < N; i++) {
        int u = i % W, v = i / W;
        if (u == maxu && v == maxv) continue;
        double m = mag2(re[i], im[i]);
        if (m > best) { best = m; bu = u; bv = v; }
    }
    out[5] = atan2((double)bv, (double)bu);
    (void)dom_num_a; (void)half;
    return 6;
}

double dft_ratio(int N, int keep) {
    double raw = (double)(N * N);
    double comp = (double)keep * 4.0 * (double)sizeof(double); /* 4 doubles */
    return raw / comp;
}
