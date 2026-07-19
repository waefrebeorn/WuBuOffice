/* goldplace.c -- golden-ratio coordinate placement (see goldplace.h).
 * Byte-identical golden math to WuBuMath/src/model/wubu_gaad_encoder.c. */
#include "goldplace.h"
#include <math.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* golden-subdivide: split a rectangle along its longer side at 1/PHI. */
static size_t golden_subdivide(GoldRect *rects, size_t nrects, int W, int H) {
    GoldRect queue[256]; size_t qh = 0, qt = 0, cnt = 0;
    if (nrects == 0) return 0;
    queue[qt++] = (GoldRect){0,0,(double)W,(double)H};
    while (qh < qt && cnt < nrects && qt < 256) {
        GoldRect r = queue[qh++];
        double w = r.x2 - r.x1, h = r.y2 - r.y1;
        if (w < 5 || h < 5) { if (cnt < nrects) rects[cnt++] = r; continue; }
        if (w > h + 0.01) {
            double c = w / GOLDPLACE_PHI;
            if (cnt < nrects) rects[cnt++] = (GoldRect){r.x1,r.y1,r.x1+c,r.y2};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1,r.y1,r.x1+c,r.y2};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1+c,r.y1,r.x2,r.y2};
        } else if (h > w + 0.01) {
            double c = h / GOLDPLACE_PHI;
            if (cnt < nrects) rects[cnt++] = (GoldRect){r.x1,r.y1,r.x2,r.y1+c};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1,r.y1,r.x2,r.y1+c};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1,r.y1+c,r.x2,r.y2};
        } else {
            double c = w / GOLDPLACE_PHI;
            if (cnt < nrects) rects[cnt++] = (GoldRect){r.x1,r.y1,r.x1+c,r.y2};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1,r.y1,r.x1+c,r.y2};
            if (qt < 256) queue[qt++] = (GoldRect){r.x1+c,r.y1,r.x2,r.y2};
        }
    }
    return cnt < nrects ? cnt : nrects;
}

/* phi-spiral: r = a * exp(LOG_PHI_2 * theta), theta steps by 2*PI/PHI. */
static size_t generate_phi_spiral(GoldPoint *pts, size_t npts, int W, int H) {
    if (npts == 0) return 0;
    double cx = (double)W / 2.0, cy = (double)H / 2.0;
    double a = 0.05 * (double)(W < H ? W : H);
    double b = GOLDPLACE_LOG_PHI_2;
    double step = GOLDPLACE_PHI * 2.0 * M_PI / (double)(npts > 1 ? npts - 1 : 1);
    double ang = 0.0;
    size_t gen = 0;
    double mr = (double)(W > H ? W : H) * 0.6;
    for (size_t i = 0; i < npts; i++) {
        double r = a * exp(b * ang);
        if (r > mr) r = mr;
        double x = cx + r * cos(ang), y = cy + r * sin(ang);
        if (x < 0) x = 0; if (x >= (double)W) x = (double)W - 1;
        if (y < 0) y = 0; if (y >= (double)H) y = (double)H - 1;
        pts[gen++] = (GoldPoint){x, y};
        ang += step;
        if (gen >= npts) break;
    }
    return gen;
}

size_t goldplace_spiral(GoldPoint *pts, size_t npts, int W, int H) {
    return generate_phi_spiral(pts, npts, W, H);
}
size_t goldplace_subdivide(GoldRect *rects, size_t nrects, int W, int H) {
    return golden_subdivide(rects, nrects, W, H);
}

size_t goldplace_layout(int W, int H, size_t nglyph, double *cx, double *cy,
                        int *region_idx, int *spiral_idx) {
    GoldRect rects[256];
    size_t nr = golden_subdivide(rects, 256, W, H);
    if (nr == 0) return 0;
    size_t placed = 0;
    /* spirals per region: ~nglyph/nr points each, distributed round-robin. */
    for (size_t i = 0; i < nglyph; i++) {
        size_t ri = i % nr;
        GoldRect *R = &rects[ri];
        int rw = (int)(R->x2 - R->x1), rh = (int)(R->y2 - R->y1);
        if (rw < 4 || rh < 4) continue;
        GoldPoint pts[8];
        size_t np = generate_phi_spiral(pts, 8, rw, rh);
        if (np == 0) continue;
        size_t si = (i / nr) % np;       /* spiral index cycles per region */
        cx[placed] = R->x1 + pts[si].x;
        cy[placed] = R->y1 + pts[si].y;
        if (region_idx) region_idx[placed] = (int)ri;
        if (spiral_idx) spiral_idx[placed] = (int)si;
        placed++;
    }
    return placed;
}
