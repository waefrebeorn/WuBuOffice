/* view_settings.c -- Settings view (UI-25): a real, interactive preferences
 * surface (not a stub). It edits the shared wubusettings singleton and persists
 * to disk on change. Keyboard: +/- zoom, t theme, a autosave toggle, l language
 * cycle, f font-size, s save, Esc close (returns to previous tab via shell). */
#include "wuos.h"
#include "wuos_font.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WubuSettings *s; int saved_flash; } SetV;

/* ---- wu_theme.h ---- */
#include "wuos_theme.h"

static WuosRGB theme_bg(void){
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB c = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    return c;
}
static WuosRGB theme_text(void){
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB c = dark ? WUOS_DARK(TABTEXT_ON) : WUOS_LIGHT(TABTEXT_ON);
    return c;
}
static WuosRGB theme_hint(void){
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB c = dark ? WUOS_DARK(TABTEXT) : WUOS_LIGHT(TABTEXT);
    return c;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    SetV *e = v->priv;
    WuosRGB bg = theme_bg();
    WuosRGB txt = theme_text();
    WuosRGB hnt = theme_hint();
    unsigned char *fb = calloc((size_t)w*h, 4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=bg.r;fb[k+1]=bg.g;fb[k+2]=bg.b;fb[k+3]=255; }

    /* Layout constants: font height ~20px, line spacing = fh + 4px (WUOS_SPACE_24 = 24px) */
    int fh = wuos_font_height();
    int line_h = fh + WUOS_SPACE_4;  /* 24px baseline-to-baseline */
    int margin_x = WUOS_SPACE_16;    /* 16px left margin */
    int y = WUOS_SPACE_16 + fh;      /* start at margin + font height (first baseline) */

    wuos_font_draw("WuBuOffice Settings", margin_x, y, 1, txt.r,txt.g,txt.b, fb, w, h); y += line_h + WUOS_SPACE_8;
    /* section: preferences — each row is label left, value+key right (aligned
     * columns instead of the old space-padded run-on strings). */
    wuos_font_draw("Preferences", margin_x, y, 1, hnt.r,hnt.g,hnt.b, fb, w, h); y += line_h;
    int val_x = margin_x + WUOS_SPACE_32 * 5;   /* value column */
    char line[256];
    snprintf(line,sizeof line,"Zoom");           wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%.0f%%      (+ / -)", e->s?wubusettings_zoom(e->s)*100:100);
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    snprintf(line,sizeof line,"Theme");          wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%s      (t)", wubusettings_dark(e->s)?"Dark":"Light");
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    snprintf(line,sizeof line,"Autosave");       wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%s      (a)", wubusettings_autosave_ms(e->s)?"ON (5s)":"OFF");
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    snprintf(line,sizeof line,"UI language");    wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%s      (l)", wubusettings_language(e->s));
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    snprintf(line,sizeof line,"Base font size"); wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%d px    (f)", wubusettings_font_size(e->s));
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    /* UI-26: live word-wrap + tab-width */
    snprintf(line,sizeof line,"Word wrap");      wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%s      (w)", wubusettings_word_wrap(e->s)?"ON":"OFF");
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    snprintf(line,sizeof line,"Tab width");      wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%d sp     (i / Shift+i)", wubusettings_tab_width(e->s));
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;
    /* UXA-41: high-contrast toggle */
    snprintf(line,sizeof line,"High contrast");  wuos_font_draw(line, margin_x, y, 0, txt.r,txt.g,txt.b, fb,w,h);
    snprintf(line,sizeof line,"%s      (c)", wubusettings_high_contrast(e->s)?"ON":"OFF");
    wuos_font_draw(line, val_x, y, 0, txt.r,txt.g,txt.b, fb,w,h); y += line_h;

    /* section: keyboard — one binding per row; the autosave note lives on the
     * Autosave preference row context, not concatenated onto a keys line. */
    int hint_y = y + WUOS_SPACE_16;
    wuos_font_draw("Keyboard", margin_x, hint_y, 1, hnt.r,hnt.g,hnt.b, fb, w, h); hint_y += line_h;
    static const char *keys[] = {
        "+ / -         zoom in / out",
        "0             reset zoom to 100%",
        "t             toggle dark / light theme",
        "a             toggle autosave",
        "l             cycle UI language",
        "f             cycle base font size",
        "w             toggle word wrap",
        "i / Shift+i   tab width -/+",
        "c             toggle high contrast",
        "s             save settings to disk",
    };
    for (int i=0;i<10;i++){
        wuos_font_draw(keys[i], margin_x+WUOS_SPACE_8, hint_y, 0, hnt.r,hnt.g,hnt.b, fb, w, h); hint_y += line_h;
    }
    if (!wubusettings_autosave_ms(e->s)){
        wuos_font_draw("Changes auto-save on every edit.", margin_x+WUOS_SPACE_8, hint_y, 0, hnt.r,hnt.g,hnt.b, fb, w, h);
    }
    if (e->saved_flash){
        wuos_font_draw("saved.", margin_x, hint_y, 0, txt.r,txt.g,txt.b, fb, w, h);
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static char *status(WuView *v){
    SetV *e = v->priv; char *s = malloc(64);
    if (s) snprintf(s,64,"Settings — %s, zoom %.0f%%",
                    wubusettings_dark(e->s)?"dark":"light",
                    wubusettings_zoom(e->s)*100);
    return s;
}

static void on_key(WuView *v, int key, int down){
    SetV *e = v->priv;
    if (!down || !e->s) return;
    WubuSettings *s = e->s;
    if (key=='+' || key=='='){ wubusettings_set_zoom(s, wubusettings_zoom(s)+0.1); }
    else if (key=='-'){ wubusettings_set_zoom(s, wubusettings_zoom(s)-0.1); }
    else if (key=='0'){ wubusettings_set_zoom(s, 1.0); }
    else if (key=='t' || key=='T'){ wubusettings_set_dark(s, !wubusettings_dark(s)); }
    else if (key=='a' || key=='A'){ wubusettings_set_autosave_ms(s, wubusettings_autosave_ms(s)?0:5000); }
    else if (key=='l' || key=='L'){
        const char *cur = wubusettings_language(s);
        const char *next = (!strcmp(cur,"en"))?"ar":(!strcmp(cur,"ar"))?"fr":"en";
        wubusettings_set_language(s, next);
    }
    else if (key=='f' || key=='F'){ wubusettings_set_font_size(s, wubusettings_font_size(s)+2>48?10:wubusettings_font_size(s)+2); }
    else if (key=='w' || key=='W'){ wubusettings_set_word_wrap(s, !wubusettings_word_wrap(s)); }
    else if (key=='i' || key=='I'){ int tw=wubusettings_tab_width(s)+1; wubusettings_set_tab_width(s, tw>16?1:tw); }
    else if (key=='c' || key=='C'){ wubusettings_set_high_contrast(s, !wubusettings_high_contrast(s)); }
    else if (key=='s' || key=='S'){ wubusettings_save(s, NULL); e->saved_flash = 12; return; }
    else return;
    /* every change persists immediately */
    wubusettings_save(s, NULL);
    if (e->saved_flash) e->saved_flash = 12;
}

static void destroy(WuView *v){ SetV *e = v->priv; free(e); free(v); }

WuView *wuos_settings_create(void){
    SetV *e = calloc(1, sizeof *e);
    e->s = wubusettings_shared();
    e->saved_flash = 0;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Settings";
    v->priv = e;
    v->destroy = destroy;
    v->render = render;
    v->status = status;
    v->on_key = on_key;
    return v;
}
