/* view_compare.c -- Compare view: diffs two files via WuBuPad's DONE Myers
 * LCS engine (src/diff) and renders a color-coded unified diff. Closes the
 * GAPS_NOTEPAD "GUI compare view" gap (engine was already built headless). */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_file.h"
#include "wuos_theme.h"
#include "diff.h"          /* cross-repo: ~/WuBuPad/src */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { char *text; char *la, *lb; } CmpV;

/* Split `s` (NUL-term) into an array of line pointers (no newline). Caller
 * frees the returned array (and the dup); lines point into `copy`. */
static char **split_lines(char *copy, size_t *n){
    size_t cap=16, cnt=0; char **arr=malloc(cap*sizeof(char*));
    char *p = copy;
    while (*p){
        if (cnt>=cap){ cap*=2; arr=realloc(arr,cap*sizeof(char*)); }
        arr[cnt++] = p;
        char *nl = strpbrk(p, "\r\n");
        if (!nl) break;
        if (*nl=='\r' && nl[1]=='\n'){ *nl=0; nl[1]=0; p=nl+2; }
        else { *nl=0; p=nl+1; }
    }
    *n = cnt;
    return arr;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    CmpV *e = v->priv;
    (void)scroll;
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB cmp_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    WuosRGB cmp_txt = dark ? WUOS_DARK(TABTEXT_ON) : WUOS_LIGHT(TABTEXT_ON);
    WuosRGB cmp_err = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=cmp_bg.r;fb[k+1]=cmp_bg.g;fb[k+2]=cmp_bg.b;fb[k+3]=255; }

    int fh = wuos_font_height();
    int lh = fh + WUOS_SPACE_4;

    if (!e->text){
        wuos_font_draw("Compare: wubuos compare <a> <b>  (no files)", WUOS_SPACE_8, WUOS_SPACE_8*3, 1, cmp_err.r,cmp_err.g,cmp_err.b, fb,w,h);
        *rgba=fb; *rw=w; *rh=h; return 0;
    }
    /* title */
    char title[256];
    snprintf(title,sizeof title,"Compare  %s  <->  %s", e->la?e->la:"(blank)", e->lb?e->lb:"(blank)");
    wuos_font_draw(title, WUOS_SPACE_8, WUOS_SPACE_4, 1, cmp_txt.r,cmp_txt.g,cmp_txt.b, fb,w,h);

    /* tokenize the unified diff into lines, paint with color by prefix */
    char *work = strdup(e->text);
    int y = 30;
    char *p = work;
    while (p && *p && y < h){
        char *nl = strpbrk(p, "\n");
        if (nl) *nl = 0;
        int r=40,g=44,b=52;        /* context (dark) */
        if (p[0]=='-' && p[1]==' '){ r=200;g=40;b=40; }      /* removed */
        else if (p[0]=='+' && p[1]==' '){ r=40;g=160;b=70; } /* added */
        else if (p[0]=='@'){ r=90;g=110;b=170; }             /* hunk header */
        else if (p[0]=='-' && p[1]=='-'){ r=90;g=110;b=170; }/* file header */
        /* clip long lines */
        char buf[256];
        size_t L = strlen(p);
        if (L > 250){ memcpy(buf,p,250); buf[250]=0; }
        else { memcpy(buf,p,L+1); }
        wuos_font_draw(buf, 10, y+fh, 0, r,g,b, fb,w,h);
        y += lh;
        if (!nl) break;
        p = nl+1;
    }
    free(work);
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static void destroy(WuView *v){ CmpV *e=v->priv; free(e->la); free(e->lb); free(e->text); free(e); free(v); }

static char *read_dup(const char *path){
    if (!path) return strdup("");
    size_t len=0; char *b = wuos_read_file(path, &len);
    if (!b) return strdup("");
    /* normalize to NUL-term */
    char *s = malloc(len+1); memcpy(s,b,len); s[len]=0; free(b);
    return s;
}

WuView *wuos_compare_create(const char *left, const char *right){
    CmpV *e = calloc(1, sizeof *e);
    e->la = left? strdup(left):NULL;
    e->lb = right? strdup(right):NULL;
    if (left && right){
        char *sa = read_dup(left), *sb = read_dup(right);
        /* build line arrays */
        size_t na=0, nb=0;
        char **la = split_lines(sa, &na);
        char **lb = split_lines(sb, &nb);
        Diff *d = diff_lines((const char*const*)la, na, (const char*const*)lb, nb);
        if (d){
            e->text = diff_unified(d, (const char*const*)la, (const char*const*)lb, left, right);
            diff_free(d);
        }
        free(la); free(lb); free(sa); free(sb);
    }
    WuView *v = calloc(1, sizeof *v);
    v->name = "Compare";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    return v;
}
