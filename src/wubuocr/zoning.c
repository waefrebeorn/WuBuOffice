/* zoning.c -- principled, interpretable glyph feature extraction.
 * See zoning.h for the feature layout. Dependency-free C11.
 *
 * IMPORTANT: features are computed over a FIXED grid on the full w x h image
 * (EMNIST already centers each glyph in 28x28). Normalizing by the tight ink
 * bounding box was removed because it stretches every glyph to fill the grid
 * and erases the spatial/size structure that distinguishes letters (A and B
 * ended up with near-identical per-cell ink fractions -> model couldn't
 * learn). A fixed grid keeps the discriminative shape information.
 *
 * Feature vector layout:
 *   [0 .. grid*grid-1]  ink-fraction per cell of a FIXED grid over the image
 *   [grid*grid]         aspect ratio (w/h) of the image canvas
 *   [grid*grid+1]       number of holes (enclosed background regions)
 *   [grid*grid+2..+5]   ink fraction in each of the 4 quadrants
 *   [grid*grid+6..+7]   center-of-mass (x,y) within the image, in [0,1]
 */
#include "zoning.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct ZoningExtractor {
    int grid;
    int dim;
};

ZoningExtractor *zoning_create(int grid) {
    if (grid < 1) grid = 1;
    ZoningExtractor *z = (ZoningExtractor *)malloc(sizeof(*z));
    if (!z) return NULL;
    z->grid = grid;
    z->dim = grid * grid + 8;
    return z;
}

void zoning_destroy(ZoningExtractor *z) { free(z); }

int zoning_dim(const ZoningExtractor *z) { return z->dim; }

/* Flood-fill background from the border; any background not reached is a hole.
 * Robust stack sizing (2*n) avoids out-of-bounds writes. */
static int count_holes(const unsigned char *px, int w, int h) {
    int n = w * h;
    if (n <= 0) return 0;
    unsigned char *bg = (unsigned char *)malloc((size_t)n);
    if (!bg) return 0;
    for (int i = 0; i < n; i++) bg[i] = (px[i] <= 127) ? 0 : 1;  /* ink=1 */

    int *stk = (int *)malloc((size_t)n * 2 * sizeof(int));
    if (!stk) { free(bg); return 0; }
    int sp = 0, cap = n * 2;

    for (int x = 0; x < w; x++) {
        if (bg[x]) { bg[x] = 0; stk[sp++] = x; }
        int b = (h - 1) * w + x;
        if (bg[b]) { bg[b] = 0; stk[sp++] = b; }
    }
    for (int y = 0; y < h; y++) {
        int l = y * w;
        if (bg[l]) { bg[l] = 0; stk[sp++] = l; }
        int r = y * w + (w - 1);
        if (bg[r]) { bg[r] = 0; stk[sp++] = r; }
    }
    while (sp > 0 && sp < cap) {
        int p = stk[--sp], cx = p % w, cy = p / w;
        int nb[4][2] = {{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
        for (int k = 0; k < 4; k++) {
            int nx = nb[k][0], ny = nb[k][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int q = ny * w + nx;
            if (bg[q]) { bg[q] = 0; stk[sp++] = q; }
        }
    }
    int holes = 0;
    for (int i = 0; i < n; i++) {
        if (bg[i]) {
            holes++;
            stk[sp++] = i; bg[i] = 0;
            while (sp > 0 && sp < cap) {
                int p = stk[--sp], cx = p % w, cy = p / w;
                int nb[4][2] = {{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
                for (int k = 0; k < 4; k++) {
                    int nx = nb[k][0], ny = nb[k][1];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    int q = ny * w + nx;
                    if (bg[q]) { bg[q] = 0; stk[sp++] = q; }
                }
            }
        }
    }
    free(stk);
    free(bg);
    return holes;
}

int zoning_extract(const ZoningExtractor *z, const unsigned char *px,
                   int w, int h, float *out) {
    int grid = z->grid;
    int dim = z->dim;
    int gdim = grid * grid;
    for (int i = 0; i < dim; i++) out[i] = 0.0f;
    if (w <= 0 || h <= 0) return dim;

    int cw = (w + grid - 1) / grid;
    int ch = (h + grid - 1) / grid;

    /* ink fraction per fixed cell over the full canvas */
    float quad[4] = {0, 0, 0, 0};
    long com_num_x = 0, com_num_y = 0, com_den = 0;
    for (int gy = 0; gy < grid; gy++)
        for (int gx = 0; gx < grid; gx++) {
            int cnt = 0, ink = 0;
            int x0 = gx * cw, y0 = gy * ch;
            for (int y = y0; y < y0 + ch && y < h; y++)
                for (int x = x0; x < x0 + cw && x < w; x++)
                    if (y >= 0 && y < h && x >= 0 && x < w) {
                        cnt++;
                        if (px[y * w + x] <= 127) {
                            ink++;
                            com_num_x += x;
                            com_num_y += y;
                            com_den++;
                            int qx = (x * 2 >= w) ? 1 : 0;
                            int qy = (y * 2 >= h) ? 1 : 0;
                            quad[qy * 2 + qx]++;
                        }
                    }
            out[gy * grid + gx] = cnt ? (float)ink / (float)cnt : 0.0f;
        }

    out[gdim]     = (h > 0) ? (float)w / (float)h : 1.0f;       /* aspect */
    out[gdim + 1] = (float)count_holes(px, w, h);                /* holes  */
    float qsum = (float)(com_den > 0 ? com_den : 1);
    out[gdim + 2] = quad[0] / qsum;
    out[gdim + 3] = quad[1] / qsum;
    out[gdim + 4] = quad[2] / qsum;
    out[gdim + 5] = quad[3] / qsum;
    out[gdim + 6] = com_den ? (float)com_num_x / (float)com_den / (float)w : 0.5f;
    out[gdim + 7] = com_den ? (float)com_num_y / (float)com_den / (float)h : 0.5f;
    return dim;
}
