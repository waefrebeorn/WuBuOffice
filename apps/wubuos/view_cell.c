/* view_cell.c -- Spreadsheet cell view: renders a real wubucell workbook grid AND
 * is genuinely interactive. A formula bar (top) shows the active cell; typing
 * edits it; Enter commits through wubucell_cell_* and the real engine
 * recomputes (e.g. SUM/A1*2 cached values update on render). Arrow keys move
 * the active cell. Loads .csv/.xlsx/.ods when given a path. */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "cell.h"          /* apps/wubucell */
#include "cell_csv.h"      /* wubucell_read_csv */
#include "cell_read.h"     /* wubucell_read */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { wubucell_book *b; int maxc, maxr;
                 int curc, curr;            /* active cell (1-based) */
                 char fbuf[256];            /* formula-bar edit buffer */
                 int editing;
                 char *path;                /* loaded path (NULL = untitled) */ } CellV;

/* Expose referenced-cell refs for testing (test_view.c). */
int wuos_cell_test_refs(const wubucell_book *b, int curc, int curr,
                        int refcol[], int refrow[], int cap);

/* Parse A1-style cell references out of the active cell's formula (Excel's
 * referenced-cell highlight: colored boxes on the cells a formula reads).
 * Fills refcol[]/refrow[] (1-based), returns the count. Handles "A1", "B12",
 * "AB3", and comma/plus/minus/star/slash/paren-separated refs. */
static int cell_refs(const CellV *e, int refcol[], int refrow[], int cap){
    if (!e || !e->b) return 0;
    return wuos_cell_test_refs(e->b, e->curc, e->curr, refcol, refrow, cap);
}

int wuos_cell_test_refs(const wubucell_book *b, int curc, int curr,
                        int refcol[], int refrow[], int cap){
    int n = 0;
    if (!b) return 0;
    wubucell_ckind k; const char *t = NULL; double num=0, c=0;
    if (wubucell_get(b, 1, curc, curr, &k, &t, &num, &c) != 0) return 0;
    if (k != WUBUCELL_FORM || !t) return 0;
    /* scan for [A-Z]+[0-9]+ tokens */
    size_t L = strlen(t);
    for (size_t i = 0; i < L; ){
        if (isalpha((unsigned char)t[i])){
            size_t j = i;
            while (j < L && isalpha((unsigned char)t[j])) j++;
            /* collect digits that directly follow the letters */
            size_t d = j;
            while (d < L && isdigit((unsigned char)t[d])) d++;
            if (d > j){   /* a cell ref: letters then digits */
                int col = 0;
                for (size_t x = i; x < j; x++) col = col*26 + (tolower((unsigned char)t[x])-'a'+1);
                int row = atoi(t+j);
                if (col >= 1 && row >= 1 && n < cap){ refcol[n]=col; refrow[n]=row; n++; }
            }
            i = (d > j) ? d : j;
        } else i++;
    }
    return n;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    CellV *e = v->priv;
    (void)scroll;
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB cel_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    WuosRGB cel_hdr = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);
    WuosRGB cel_body = dark ? WUOS_DARK(TABTEXT_ON) : WUOS_LIGHT(TABTEXT_ON);
    WuosRGB cel_edge = dark ? WUOS_DARK(BORDER) : WUOS_LIGHT(BORDER);
    WuosRGB cel_active = dark ? WUOS_DARK(OVERLAY_HIGHLIGHT) : WUOS_LIGHT(OVERLAY_HIGHLIGHT);
    WuosRGB cel_fx_bg = dark ? (WuosRGB){36,40,47} : (WuosRGB){233,236,241};  /* SURFACE_3 */
    WuosRGB cel_fx_edge = dark ? WUOS_DARK(BORDER) : WUOS_LIGHT(BORDER);
    WuosRGB cel_fx_text = dark ? WUOS_DARK(OVERLAY_TEXT) : WUOS_LIGHT(OVERLAY_TEXT);
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=cel_bg.r;fb[k+1]=cel_bg.g;fb[k+2]=cel_bg.b;fb[k+3]=255; }

    int fh = wuos_font_height();
    int lh = fh + WUOS_SPACE_8;
    int margin_x = WUOS_SPACE_8 * 5;    /* 40px */
    int margin_y = WUOS_SPACE_8 * 7;    /* 56px */
    int cw = WUOS_SPACE_8 * 12;         /* 96px cell width */
    int rh2 = lh;

    /* formula bar area: WUOS_SPACE_8*3 (24px) high, starts at margin_y - lh - WUOS_SPACE_8 */
    int fx_y0 = margin_y - lh - WUOS_SPACE_8;
    int fx_h = lh + WUOS_SPACE_8;  /* formula bar height */

    /* "fx" label */
    wuos_font_draw("fx", margin_x, fx_y0 + WUOS_SPACE_4 + fh, 1, cel_hdr.r,cel_hdr.g,cel_hdr.b, fb,w,h);

    /* formula bar background */
    int fx_x0 = margin_x + WUOS_SPACE_8 * 3;  /* after "fx " */
    int fx_x1 = w - WUOS_SPACE_8;
    for (int xx=fx_x0; xx<fx_x1; xx++)
        for (int yy=fx_y0; yy<fx_y0+fx_h; yy++)
            if (xx>=0 && yy>=0 && xx<w && yy<h){
                size_t i=((size_t)yy*w+xx)*4;
                int edge = (xx==fx_x0 || yy==fx_y0 || xx==fx_x1-1 || yy==fx_y0+fx_h-1);
                fb[i] = edge ? cel_fx_edge.r : cel_fx_bg.r;
                fb[i+1] = edge ? cel_fx_edge.g : cel_fx_bg.g;
                fb[i+2] = edge ? cel_fx_edge.b : cel_fx_bg.b;
            }

    /* cell address label (A1, B2, etc.) */
    char ab[8]; int cc=e->curc-1; ab[0]=(cc<26)?'A'+cc:('A'+(cc/26)-1);
    if(cc>=26) ab[1]='A'+(cc%26); else ab[1]=0;
    char lbl[16]; snprintf(lbl,sizeof lbl,"%s%d",ab,e->curr);
    wuos_font_draw(lbl, fx_x0 + WUOS_SPACE_4, fx_y0 + WUOS_SPACE_4 + fh, 0, cel_fx_text.r,cel_fx_text.g,cel_fx_text.b, fb,w,h);

    /* cell content in formula bar: show the FORMULA string for a formula cell
     * (Excel parity), else the text/value. The computed result stays in the
     * grid. */
    const char *shown = e->editing ? e->fbuf : "";
    if (!e->editing){
        wubucell_ckind k; const char *t=NULL; double num=0,c=0;
        if (wubucell_get(e->b,1,e->curc,e->curr,&k,&t,&num,&c)==0){
            static char disp[80];
            if (k==WUBUCELL_FORM && t){
                snprintf(disp,sizeof disp,"=%s",t);   /* show the formula */
            } else if (k==WUBUCELL_FORM){
                snprintf(disp,sizeof disp,"%g",c);
            } else if (k==WUBUCELL_NUM){
                snprintf(disp,sizeof disp,"%g",num);
            } else snprintf(disp,sizeof disp,"%s",t?t:"");
            shown = disp;
        }
    }
    wuos_font_draw(shown, fx_x0 + WUOS_SPACE_8 * 6, fx_y0 + WUOS_SPACE_4 + fh, 0, cel_fx_text.r,cel_fx_text.g,cel_fx_text.b, fb,w,h);

    /* column headers (A, B, C...) */
    for (int c=1; c<=e->maxc; c++){
        char lab[8]; int cc=c-1; lab[0]=(cc<26)?'A'+cc:(('A'+(cc/26)-1)); if(cc>=26) lab[1]='A'+(cc%26); else lab[1]=0;
        int on=(c==e->curc);
        WuosRGB hdr_col = on ? cel_active : cel_hdr;
        wuos_font_draw(lab, margin_x + c*cw + WUOS_SPACE_4, margin_y + fh, 0, hdr_col.r,hdr_col.g,hdr_col.b, fb,w,h);
    }

    /* row headers + cells */
    /* Excel referenced-cell highlight: colored boxes on cells the active
     * formula reads (distinct from the active-cell fill). */
    int refcol[32], refrow[32];
    int nref = cell_refs(e, refcol, refrow, 32);
    WuosRGB cel_ref = dark ? (WuosRGB){120,190,255} : (WuosRGB){0,110,215};
    for (int r=1; r<=e->maxr; r++){
        char rn[16]; snprintf(rn,sizeof rn,"%d",r);
        int on=(r==e->curr);
        WuosRGB hdr_col = on ? cel_active : cel_hdr;
        wuos_font_draw(rn, margin_x - WUOS_SPACE_8 * 4, margin_y + r*rh2 + fh, 0, hdr_col.r,hdr_col.g,hdr_col.b, fb,w,h);
        for (int c=1; c<=e->maxc; c++){
            wubucell_ckind k; const char *t=NULL; double num=0, cached=0;
            int has = wubucell_get(e->b, 1, c, r, &k, &t, &num, &cached);
            int x = margin_x + c*cw;
            int y = margin_y + r*rh2;
            int active = (c==e->curc && r==e->curr);
            int referenced = 0;
            for (int ri=0; ri<nref; ri++) if (refcol[ri]==c && refrow[ri]==r){ referenced=1; break; }
            for (int yy=y; yy<y+rh2; yy++) {
                for (int xx=x; xx<x+cw; xx++) {
                    if (xx>=w||yy>=h) continue;
                    size_t i=((size_t)yy*w+xx)*4;
                    int edge = (xx==x||yy==y||xx==x+cw-1||yy==y+rh2-1);
                    if (active){
                        fb[i]=cel_active.r; fb[i+1]=cel_active.g; fb[i+2]=cel_active.b;
                    } else {
                        WuosRGB cell_bg = edge ? cel_edge : cel_bg;
                        fb[i]=cell_bg.r; fb[i+1]=cell_bg.g; fb[i+2]=cell_bg.b;
                    }
                    /* overlay a 2px referenced-cell ring */
                    if (referenced && (xx==x||xx==x+1||yy==y||yy==y+1||
                                       xx==x+cw-2||xx==x+cw-1||yy==y+rh2-2||yy==y+rh2-1)){
                        fb[i]=cel_ref.r; fb[i+1]=cel_ref.g; fb[i+2]=cel_ref.b;
                    }
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
                wuos_font_draw(buf, x+WUOS_SPACE_4, y+fh, 0, cel_body.r,cel_body.g,cel_body.b, fb,w,h);
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

/* Navigator sidebar content: the active cell + populated cells in its row.
 * Real values via wubucell_get; NULL if no book. Caller frees. */
static char *sidebar(WuView *v){
    CellV *e = v->priv;
    if (!e->b) return NULL;
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    char hdr[48]; snprintf(hdr, sizeof hdr, "Active: %c%d\n", 'A'+(e->curc-1), e->curr);
    size_t add = strlen(hdr); memcpy(out+len, hdr, add); len+=add; out[len]=0;
    /* list populated cells in the current row (cols 1..40) */
    for (int c=1;c<=40;c++){
        wubucell_ckind k; const char *txt=NULL; double num=0, cached=0;
        if (wubucell_get(e->b, 1, c, e->curr, &k, &txt, &num, &cached) == 0){
            char line[96];
            snprintf(line, sizeof line, "%c%d: %s%s\n", 'A'+(c-1), e->curr,
                     k==WUBUCELL_FORM ? "=" : "", txt?txt:"");
            add = strlen(line);
            if (len+add+1 > cap){ cap=(len+add+1)*2; char *nb=realloc(out,cap); if(!nb){ free(out); return NULL; } out=nb; }
            memcpy(out+len, line, add); len+=add; out[len]=0;
        }
    }
    return out;
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

static void destroy(WuView *v){ CellV *e = v->priv; wubucell_free(e->b); free(e->path); free(e); free(v); }

static const char *get_path(WuView *v){ CellV *e = v->priv; return e->path; }

/* Save the spreadsheet back to its loaded path (round-trip). CSV writes sheet
 * 1 as RFC-4180 text; XLSX re-assembles. */
static void save(WuView *v){
    CellV *e = v->priv;
    if (!e || !e->path) return;
    size_t L = strlen(e->path);
    if (L > 4 && !strcasecmp(e->path + L - 4, ".csv"))
        wubucell_write_csv(e->b, 1, ',', e->path);
    else
        wubucell_assemble(e->b, e->path);   /* .xlsx/.ods (or default) */
}

WuView *wuos_cell_create(const char *path){
    CellV *e = calloc(1, sizeof *e);
    e->curc = 1; e->curr = 1; e->editing = 0; e->fbuf[0]=0;
    if (path){
        e->path = strdup(path);
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
    v->sidebar = sidebar;
    v->on_key  = on_key;
    v->get_path = get_path;
    v->save    = save;   /* round-trip: Ctrl+S writes back to the loaded format */
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
