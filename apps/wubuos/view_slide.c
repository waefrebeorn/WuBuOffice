/* view_slide.c -- Slide view: an EDITABLE, SAVABLE presentation slide
 * (title + bullets + a bar chart). No longer a dead static stub: the title and
 * bullets live in a real model, can be edited with the keyboard, and save/load
 * to a simple line-based text format (one slide per file). Full .pptx/.odp
 * OOXML round-trip is a larger feature; this closes the "see, edit, save"
 * gap for the slide surface itself.
 *
 * Format (line-based, human-editable):
 *   WuBuSlide 1            <- magic + version
 *   T: <title>
 *   B: <bullet>            <- repeat
 *   # chart                <- optional bar-chart section (label:value)
 *   D: <label>:<value>
 */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "hive.h"        /* data-driven slide template (no hardcoded content) */
#include "../../src/wubuoxml/pptx_write.h"  /* H13: real .pptx assembly */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SLIDE_MAX_BULLETS 12
#define SLIDE_MAX_TITLE   96
#define SLIDE_MAX_CHART   8

typedef struct {
    char title[SLIDE_MAX_TITLE];
    char bullets[SLIDE_MAX_BULLETS][96];
    int  nbullets;
    double chart[SLIDE_MAX_CHART];
    int  nchart;
    int  sel;              /* 0 = title, 1..n = bullet being edited */
    int  caret;
    int  editing;
    char *path;            /* loaded path (NULL = untitled) */
} SlideV;

static int dark_mode(void){ return wubusettings_dark(wubusettings_shared()); }

/* ---- render ---- */
static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    SlideV *e = v->priv;
    int dark = dark_mode();
    WuosRGB sld_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    WuosRGB sld_accent = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
    WuosRGB sld_body = dark ? WUOS_DARK(OVERLAY_TEXT) : WUOS_LIGHT(OVERLAY_TEXT);
    WuosRGB sld_hd = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=sld_bg.r;fb[k+1]=sld_bg.g;fb[k+2]=sld_bg.b;fb[k+3]=255; }

    int accent_h = WUOS_SPACE_8;
    for (int y=0; y<accent_h; y++) for (int x=0; x<w; x++){
        size_t i=((size_t)y*w+x)*4; fb[i]=sld_accent.r; fb[i+1]=sld_accent.g; fb[i+2]=sld_accent.b; }

    int fh = wuos_font_height();
    int line_h = fh + WUOS_SPACE_4;
    int margin_x = WUOS_SPACE_8 * 6;

    int title_y = accent_h + WUOS_SPACE_8 * 4 + fh;
    /* edit indicator on the title/bullet being edited */
    int seldim = (e->editing && e->sel == 0) ? 160 : 0;
    if (seldim){ for (int x=0; x<w; x++){ size_t i=((size_t)title_y*w+x)*4; fb[i]=seldim; fb[i+1]=seldim; fb[i+2]=seldim; } }
    wuos_font_draw(e->title[0] ? e->title : "(untitled slide — press Enter to edit title)",
                   margin_x, title_y, 1, sld_hd.r,sld_hd.g,sld_hd.b, fb,w,h);

    int y = title_y + WUOS_SPACE_8 * 4;
    for (int i=0; i<e->nbullets; i++){
        seldim = (e->editing && e->sel == i+1) ? 160 : 0;
        if (seldim){ for (int x=0; x<w; x++){ size_t ii=((size_t)y*w+x)*4; fb[ii]=seldim; fb[ii+1]=seldim; fb[ii+2]=seldim; } }
        wuos_font_draw("-", margin_x + WUOS_SPACE_8, y, 0, sld_accent.r,sld_accent.g,sld_accent.b, fb,w,h);
        wuos_font_draw(e->bullets[i], margin_x + WUOS_SPACE_8 * 2, y, 0, sld_body.r,sld_body.g,sld_body.b, fb,w,h);
        y += line_h;
    }
    if (e->nbullets == 0 && !e->editing){
        wuos_font_draw("- (no bullets — press Enter on a line to add one)", margin_x + WUOS_SPACE_8, y, 0, sld_body.r*0.7,sld_body.g*0.7,sld_body.b*0.7, fb,w,h);
    }

    /* bar chart (data from the hive template — no hardcoded values) */
    int chart_margin = WUOS_SPACE_8 * 6;
    int cx0 = chart_margin;
    int cy0 = y + WUOS_SPACE_8 * 4;
    int cw = w - chart_margin * 2;
    int chh = WUOS_SPACE_8 * 20;
    int n = e->nchart; double maxv = 1;
    for (int i = 0; i < n; i++) if (e->chart[i] > maxv) maxv = e->chart[i];
    int bw = n ? (cw / n - WUOS_SPACE_8 * 2) : 0;
    for (int i=0; i<n && i<SLIDE_MAX_CHART; i++){
        int bx = cx0 + WUOS_SPACE_8 + i * (cw / n);
        int bh = (int)(chh * (e->chart[i] / maxv));
        for (int yy = cy0 + chh - bh; yy < cy0 + chh; yy++)
            for (int xx = bx; xx < bx + bw; xx++)
                if (xx >= 0 && yy >= 0 && xx < w && yy < h){
                    size_t ii = ((size_t)yy * w + xx) * 4;
                    fb[ii] = sld_accent.r; fb[ii+1] = sld_accent.g; fb[ii+2] = sld_accent.b;
                }
    }
    for (int x = cx0; x < cx0 + cw; x++){
        size_t ii = ((size_t)(cy0 + chh) * w + x) * 4;
        fb[ii] = sld_body.r; fb[ii+1] = sld_body.g; fb[ii+2] = sld_body.b;
    }

    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

/* ---- editing ---- */
static void on_key(WuView *v, int key, int down){
    SlideV *e = v->priv;
    if (!down) return;
    if (key == WUOS_KEY_SAVE){ if (v->save) v->save(v); return; }
    if (key == WUOS_KEY_TAB){ e->editing = 0; e->sel++; if (e->sel > e->nbullets) e->sel = e->nbullets; return; }
    if (key == WUOS_KEY_ESC){ e->editing = 0; return; }
    if (e->editing){
        char *dst = (e->sel == 0) ? e->title : e->bullets[e->sel-1];
        size_t cap = (e->sel == 0) ? sizeof e->title : sizeof e->bullets[0];
        if (key == WUOS_KEY_BACKSPACE){
            size_t L = strlen(dst); if (L) dst[L-1] = 0;
        } else if (key >= 32 && key < 128 && strlen(dst) < cap-1){
            size_t L = strlen(dst); dst[L] = (char)key; dst[L+1] = 0;
        }
        return;
    }
    /* not editing: Enter starts editing the selected line */
    if (key == WUOS_KEY_RETURN || key == WUOS_KEY_DOWN){
        if (key == WUOS_KEY_RETURN && e->sel == e->nbullets && e->nbullets < SLIDE_MAX_BULLETS){
            e->bullets[e->nbullets][0] = 0; e->nbullets++; e->sel = e->nbullets;
        }
        if (e->sel < e->nbullets) e->sel++;
        e->editing = 1;
        return;
    }
    if (key == WUOS_KEY_UP){ if (e->sel > 0) e->sel--; return; }
}

/* ---- persistence ---- */
static const char *get_path(WuView *v){ return ((SlideV*)v->priv)->path; }

static void save(WuView *v){
    SlideV *e = v->priv;
    const char *out = e->path ? e->path : "/tmp/wubuos_slide.txt";
    /* H13: a .pptx path assembles a real PowerPoint package */
    size_t oL = strlen(out);
    if (oL > 5 && !strcasecmp(out + oL - 5, ".pptx")){
        const char *bl[SLIDE_MAX_BULLETS];
        for (int i = 0; i < e->nbullets; i++) bl[i] = e->bullets[i];
        wubuoxml_pptx_write(out, e->title, bl, e->nbullets);
        return;
    }
    FILE *f = fopen(out, "w");
    if (!f) return;
    fprintf(f, "WuBuSlide 1\n");
    fprintf(f, "T: %s\n", e->title);
    for (int i=0; i<e->nbullets; i++) fprintf(f, "B: %s\n", e->bullets[i]);
    fprintf(f, "# chart\n");
    for (int i=0; i<e->nchart; i++) fprintf(f, "D: V%d:%g\n", i+1, e->chart[i]);
    fclose(f);
}

static void load_slide(SlideV *e, const char *path){
    /* H18: real .pptx files load through the OOXML reader */
    size_t pl = strlen(path);
    if (pl > 5 && !strcasecmp(path + pl - 5, ".pptx")){
        e->nbullets = 0;
        char tmp_t[SLIDE_MAX_TITLE];
        if (wubuoxml_pptx_read(path, tmp_t, sizeof tmp_t,
                               e->bullets, SLIDE_MAX_BULLETS,
                               &e->nbullets) == 0)
            snprintf(e->title, sizeof e->title, "%s", tmp_t);
        return;
    }
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)){
        if (!strncmp(line, "T: ", 3)){ char *nl=strchr(line+3,'\n'); if(nl)*nl=0; char *cr=strchr(line+3,'\r'); if(cr)*cr=0; snprintf(e->title, sizeof e->title, "%.*s", (int)sizeof e->title - 1, line+3); }
        else if (!strncmp(line, "B: ", 3) && e->nbullets < SLIDE_MAX_BULLETS){
            char *nl = strchr(line+3, '\n'); if (nl) *nl = 0;
            char *cr = strchr(line+3, '\r'); if (cr) *cr = 0;
            snprintf(e->bullets[e->nbullets], sizeof e->bullets[0], "%.*s",
                     (int)sizeof e->bullets[0] - 1, line+3);
            e->nbullets++;
        }
        else if (!strncmp(line, "D: ", 3) && e->nchart < SLIDE_MAX_CHART){
            /* chart datum "D: V1:40" -> value after the last ':' */
            char *colon = strrchr(line+3, ':');
            if (colon) e->chart[e->nchart++] = atof(colon+1);
        }
    }
    fclose(f);
}

static void destroy(WuView *v){ SlideV *e = v->priv; free(e->path); free(e); free(v); }

WuView *wuos_slide_create(const char *path){
    SlideV *e = calloc(1, sizeof *e);
    WuView *v = calloc(1, sizeof *v);
    e->sel = 0;
    /* seed from the data-driven hive template (title/bullets/chart) */
    Hive *hive = hive_load();
    const HiveSlide *hs = hive ? hive_slide(hive) : NULL;
    if (hs){
        if (hs->title) snprintf(e->title, sizeof e->title, "%s", hs->title);
        for (size_t i = 0; i < hs->nbullets && i < SLIDE_MAX_BULLETS; i++)
            snprintf(e->bullets[e->nbullets++], sizeof e->bullets[0], "%s", hs->bullets[i]);
        for (size_t i = 0; i < hs->nchart && i < SLIDE_MAX_CHART; i++)
            e->chart[e->nchart++] = hs->chart[i];
    }
    if (hive) hive_free(hive);
    if (path){
        e->path = strdup(path);
        load_slide(e, path);   /* a saved slide file overrides the template */
    }
    v->name = "Slide";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    v->on_key  = on_key;
    v->get_path = get_path;
    v->save    = save;
    return v;
}
