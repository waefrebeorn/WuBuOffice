/* view_cell.c -- Spreadsheet view: renders a real wubucell workbook grid AND
 * is genuinely interactive. A formula bar (top) shows the active cell; typing
 * edits it; Enter commits through wubucell_cell_* and the real engine
 * recomputes (e.g. SUM/A1*2 cached values update on render). Arrow keys move
 * the active cell. Loads .csv/.xlsx/.ods when given a path. */
#include "wuos.h"
#include "wuos_font.h"
#include "cell.h"          /* apps/wubucell */
#include "cell_csv.h"      /* wubucell_read_csv */
#include "cell_read.h"     /* wubucell_read */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

typedef struct { wubucell_book *b; int maxc, maxr;
                 int curc, curr;            /* active cell (1-based) */
                 char fbuf[256];            /* formula-bar edit buffer */
                 int editing; } CellV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    CellV *e = v->priv;
    (void)scroll;
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ fb[i*4]=255;fb[i*4+1]=255;fb[i*4+2]=255;fb[i*4+3]=255; }

    int fh = wuos_font_height();
    int lh = fh + 8;
    int gx0 = 40, gy0 = 56, cw = 96, rh2 = lh;

    /* formula bar */
    wuos_font_draw("fx", 8, gy0-lh-2, 1, 90,94,102, fb,w,h);
    for (int xx=30; xx<w-10; xx++) for (int yy=gy0-lh-8; yy<gy0-4; yy++){
        if (xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4;
            int edge=(xx==30||yy==gy0-lh-8||xx==w-11||yy==gy0-5);
            fb[i]=edge?180:248; fb[i+1]=edge?184:250; fb[i+2]=edge?190:252; }
    }
    char ab[8]; int cc=e->curc-1; ab[0]=(cc<26)?'A'+cc:('A'+(cc/26)-1);
    if(cc>=26) ab[1]='A'+(cc%26); else ab[1]=0;
    char lbl[16]; snprintf(lbl,sizeof lbl,"%s%d",ab,e->curr);
    wuos_font_draw(lbl, 32, gy0-lh-4, 0, 60,66,74, fb,w,h);
    const char *shown = e->editing ? e->fbuf : "";
    if (!e->editing){
        /* show the formula/text of the active cell when not editing */
        wubucell_ckind k; const char *t=NULL; double num=0,c=0;
        if (wubucell_get(e->b,1,e->curc,e->curr,&k,&t,&num,&c)==0){
            static char disp[80];
            if (k==WUBUCELL_NUM||k==WUBUCELL_FORM) snprintf(disp,sizeof disp,"%g",(k==WUBUCELL_FORM&&c)?c:num);
            else snprintf(disp,sizeof disp,"%s",t?t:"");
            shown = disp;
        }
    }
    wuos_font_draw(shown, 70, gy0-lh-4, 0, 20,24,30, fb,w,h);

    /* column headers */
    for (int c=1;c<=e->maxc;c++){
        char lab[8]; int cc=c-1; lab[0]=(cc<26)?'A'+cc:(('A'+(cc/26)-1)); if(cc>=26) lab[1]='A'+(cc%26); else lab[1]=0;
        int on=(c==e->curc);
        wuos_font_draw(lab, gx0 + c*cw + 4, gy0, 0, on?40:110, on?70:114, on?120:120, fb,w,h);
    }

    for (int r=1;r<=e->maxr;r++){
        char rn[16]; snprintf(rn,sizeof rn,"%d",r);
        int on=(r==e->curr);
        wuos_font_draw(rn, gx0-30, gy0 + r*rh2 + fh, 0, on?40:110, on?70:114, on?120:120, fb,w,h);
        for (int c=1;c<=e->maxc;c++){
            wubucell_ckind k; const char *t=NULL; double num=0, cached=0;
            int has = wubucell_get(e->b, 1, c, r, &k, &t, &num, &cached);
            int x = gx0 + c*cw, y = gy0 + r*rh2;
            int active = (c==e->curc && r==e->curr);
            for (int yy=y; yy<y+rh2; yy++) {
                for (int xx=x; xx<x+cw; xx++) {
                    if (xx>=w||yy>=h) continue;
                    size_t i=((size_t)yy*w+xx)*4;
                    int edge = (xx==x||yy==y||xx==x+cw-1||yy==y+rh2-1);
                    if (active){ fb[i]=210;fb[i+1]=232;fb[i+2]=255; }
                    else { fb[i]=edge?200:255; fb[i+1]=edge?203:255; fb[i+2]=edge?208:255; }
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

static char *status(WuView *v){
    CellV *e = v->priv;
    char *s = malloc(128);
    if (!s) return NULL;
    snprintf(s,128,"Cell %c%d — arrows move, type to edit, Enter commits",
            'A'+(e->curc-1), e->curr);
    return s;
}

static void on_key(WuView *v, int key, int down){
    CellV *e = v->priv;
    if (!down) return;
    if (e->editing){
        if (key==WUOS_KEY_RETURN || key==WUOS_KEY_TAB){
            /* commit */
            if (e->fbuf[0]=='=' || strchr(e->fbuf,'+') || strchr(e->fbuf,'*') || strchr(e->fbuf,'(')){
                wubucell_cell_f(e->b, 1, e->curc, e->curr, e->fbuf[0]=='='?e->fbuf+1:e->fbuf, 0);
            } else if (e->fbuf[0] && (e->fbuf[0]=='-'||(e->fbuf[0]>='0'&&e->fbuf[0]<='9'))){
                wubucell_cell_n(e->b, 1, e->curc, e->curr, atof(e->fbuf));
            } else if (e->fbuf[0]){
                wubucell_cell_s(e->b, 1, e->curc, e->curr, e->fbuf);
            }
            e->editing = 0; e->fbuf[0]=0;
            if (key==WUOS_KEY_TAB){ if(e->curc<e->maxc) e->curc++; }
            return;
        }
        if (key==WUOS_KEY_BACKSPACE){
            size_t L=strlen(e->fbuf); if(L) e->fbuf[L-1]=0; return;
        }
        if (key>=32 && key<128 && strlen(e->fbuf)<(sizeof e->fbuf-1)){
            size_t L=strlen(e->fbuf); e->fbuf[L]=(char)key; e->fbuf[L+1]=0; return;
        }
        return;
    }
    /* navigation / start edit */
    if (key==WUOS_KEY_LEFT)  { if(e->curc>1) e->curc--; return; }
    if (key==WUOS_KEY_RIGHT) { if(e->curc<e->maxc) e->curc++; return; }
    if (key==WUOS_KEY_UP)    { if(e->curr>1) e->curr--; return; }
    if (key==WUOS_KEY_DOWN)  { if(e->curr<e->maxr) e->curr++; return; }
    if (key==WUOS_KEY_RETURN){ e->editing=1; e->fbuf[0]=0; return; }
    if (key>=32 && key<128){ e->editing=1; e->fbuf[0]=(char)key; e->fbuf[1]=0; return; }
}

static void destroy(WuView *v){ CellV *e = v->priv; wubucell_free(e->b); free(e); free(v); }

static const char *get_path(WuView *v){ (void)v; return NULL; }

WuView *wuos_cell_create(const char *path){
    CellV *e = calloc(1, sizeof *e);
    e->curc = 1; e->curr = 1; e->editing = 0; e->fbuf[0]=0;
    if (path){
        const char *dot = strrchr(path, '.');
        if (dot && !strcasecmp(dot, ".csv")) wubucell_read_csv(path, ',', &e->b);
        else if (dot && (!strcasecmp(dot, ".xlsx")||!strcasecmp(dot, ".ods")))
            wubucell_read(path, &e->b);
    }
    if (!e->b){
        e->b = wubucell_create();
        int s = wubucell_sheet(e->b, "Sheet1");
        wubucell_cell_n(e->b, s, 1, 1, 10);
        wubucell_cell_n(e->b, s, 2, 1, 24);
        wubucell_cell_n(e->b, s, 3, 1, 15);
        wubucell_cell_n(e->b, s, 4, 1, 30);
        wubucell_cell_f(e->b, s, 5, 1, "SUM(A1:D1)", 79);
        wubucell_cell_s(e->b, s, 1, 2, "Total revenue");
        wubucell_cell_f(e->b, s, 2, 2, "A1*2", 158);
        wubucell_cell_s(e->b, s, 1, 3, "Quarter");
        wubucell_chart(e->b, s, "Revenue", "Sheet1!A1:D1", "Sheet1!A1:D1");
    }
    int s = 1;
    wubucell_sheet_dims(e->b, s, &e->maxc, &e->maxr);
    if (e->maxr < 3) e->maxr = 3;
    if (e->maxc < 5) e->maxc = 5;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Spreadsheet";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    v->status  = status;
    v->on_key  = on_key;
    v->get_path = get_path;
    return v;
}

/* ---- test accessors ---- */
int wuos_cell_active(WuView *v, int *col, int *row){
    CellV *e = v->priv; if(col)*col=e->curc; if(row)*row=e->curr; return 0;
}
int wuos_cell_editing(WuView *v){ return ((CellV*)v->priv)->editing; }
void wuos_cell_value(WuView *v, char *out, int outn){
    CellV *e = v->priv; out[0]=0;
    wubucell_ckind k; const char *t=NULL; double num=0,c=0;
    if (wubucell_get(e->b,1,e->curc,e->curr,&k,&t,&num,&c)==0){
        if (k==WUBUCELL_NUM||k==WUBUCELL_FORM) snprintf(out,outn,"%g",(k==WUBUCELL_FORM&&c)?c:num);
        else snprintf(out,outn,"%s",t?t:"");
    }
}
int wuos_cell_kind(WuView *v){
    CellV *e = v->priv;
    wubucell_ckind k; const char *t=NULL; double num=0,c=0;
    if (wubucell_get(e->b,1,e->curc,e->curr,&k,&t,&num,&c)!=0) return -1;
    return (int)k;
}
void wuos_cell_formula(WuView *v, char *out, int outn){
    CellV *e = v->priv; out[0]=0;
    wubucell_ckind k; const char *t=NULL; double num=0,c=0;
    if (wubucell_get(e->b,1,e->curc,e->curr,&k,&t,&num,&c)==0 && k==WUBUCELL_FORM && t)
        snprintf(out,outn,"%s",t);
}
