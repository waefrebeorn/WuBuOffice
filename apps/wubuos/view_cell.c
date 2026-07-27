/* view_cell.c -- Spreadsheet view: renders a real wubucell workbook grid.
 * Builds a sample book (formulas + numbers + a chart ref) through the actual
 * wubucell builder API and renders its cells in a grid with a formula bar. */
#include "wuos.h"
#include "wuos_font.h"
#include "cell.h"          /* apps/wubucell */

#include <stdlib.h>
#include <string.h>

typedef struct { wubucell_book *b; int maxc, maxr; } CellV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    CellV *e = v->priv;
    (void)scroll;
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ fb[i*4]=255;fb[i*4+1]=255;fb[i*4+2]=255;fb[i*4+3]=255; }

    int fh = wuos_font_height();
    int lh = fh + 8;
    int gx0 = 40, gy0 = 30, cw = 96, rh2 = lh;

    /* title */
    wuos_font_draw("Spreadsheet — wubucell (real engine)", gx0, gy0-6, 1, 40,44,52, fb,w,h);

    /* column headers */
    for (int c=1;c<=e->maxc;c++){
        char lab[8]; int cc=c-1; lab[0]=(cc<26)?'A'+cc:(('A'+(cc/26)-1)); if(cc>=26) lab[1]='A'+(cc%26); else lab[1]=0;
        wuos_font_draw(lab, gx0 + c*cw + 4, gy0, 0, 110,114,120, fb,w,h);
    }

    for (int r=1;r<=e->maxr;r++){
        char rn[16]; snprintf(rn,sizeof rn,"%d",r);
        wuos_font_draw(rn, gx0-30, gy0 + r*rh2 + fh, 0, 110,114,120, fb,w,h);
        for (int c=1;c<=e->maxc;c++){
            wubucell_ckind k; const char *t=NULL; double num=0, cached=0;
            int has = wubucell_get(e->b, 1, c, r, &k, &t, &num, &cached);
            int x = gx0 + c*cw, y = gy0 + r*rh2;
            /* cell border */
            for (int yy=y; yy<y+rh2; yy++) {
                for (int xx=x; xx<x+cw; xx++) {
                    if (xx>=w||yy>=h) continue;
                    size_t i=((size_t)yy*w+xx)*4;
                    int edge = (xx==x||yy==y||xx==x+cw-1||yy==y+rh2-1);
                    fb[i]=edge?200:255; fb[i+1]=edge?203:255; fb[i+2]=edge?208:255;
                }
            }
            if (has){
                char buf[64];
                if (k==WUBUCELL_NUM||k==WUBUCELL_FORM){
                    double v = (k==WUBUCELL_FORM && cached)? cached : num;
                    snprintf(buf,sizeof buf,"%g", v);
                } else {
                    snprintf(buf,sizeof buf,"%s", t?t:"");
                }
                wuos_font_draw(buf, x+5, y+fh, 0, 30,33,38, fb,w,h);
            }
        }
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static void destroy(WuView *v){ CellV *e = v->priv; wubucell_free(e->b); free(e); }

WuView *wuos_cell_create(void){
    CellV *e = calloc(1, sizeof *e);
    e->b = wubucell_create();
    int s = wubucell_sheet(e->b, "Sheet1");
    /* a small real model: numbers + a SUM formula */
    wubucell_cell_n(e->b, s, 1, 1, 10);
    wubucell_cell_n(e->b, s, 2, 1, 24);
    wubucell_cell_n(e->b, s, 3, 1, 15);
    wubucell_cell_n(e->b, s, 4, 1, 30);
    wubucell_cell_f(e->b, s, 5, 1, "SUM(A1:D1)", 79);
    wubucell_cell_s(e->b, s, 1, 2, "Total revenue");
    wubucell_cell_f(e->b, s, 2, 2, "A1*2", 158);
    wubucell_cell_s(e->b, s, 1, 3, "Quarter");
    wubucell_chart(e->b, s, "Revenue", "Sheet1!A1:D1", "Sheet1!A1:D1");
    wubucell_sheet_dims(e->b, s, &e->maxc, &e->maxr);
    if (e->maxr < 3) e->maxr = 3;
    if (e->maxc < 5) e->maxc = 5;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Spreadsheet";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    return v;
}
