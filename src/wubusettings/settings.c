/* settings.c -- see settings.h. Opaque config with JSON persistence. */
#include "settings.h"
#include "json.h"   /* wubujson: clean-room JSON (no third-party dep) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_PATH "~/.wubuos/settings.json"

struct WubuSettings {
    double zoom;
    int    dark;
    int    autosave_ms;
    char   language[8];
    int    font_size;
    int    word_wrap;         /* UI-26: soft word-wrap (1 on) */
    int    tab_width;         /* editor tab stop in spaces (1..16) */
    int    high_contrast;
    int    reduced_motion;   /* DOC-43: disable animations/transitions */
    double ui_scale;          /* DOC-45: UI chrome scale, independent of doc zoom */
    int    first_run;         /* UI-30: show first-run splash until dismissed */
    char   font_family[64];    /* INT-15: preferred font family name */
    char   recents[16][256];   /* UI-39: recent-doc paths (most-recent-first) */
    int    nrecents;
};

WubuSettings *wubusettings_create(void){
    WubuSettings *s = calloc(1, sizeof(WubuSettings));
    if (!s) return NULL;
    s->zoom        = 1.0;
    s->dark        = 1;
    s->autosave_ms = 5000;
    s->language[0] = 'e'; s->language[1] = 'n'; s->language[2] = '\0';
    s->font_size   = 16;
    s->word_wrap  = 1;   /* UI-26: wrap on by default */
    s->tab_width  = 4;   /* classic editor default */
    s->high_contrast = 0;
    s->reduced_motion = 0;
    s->ui_scale = 1.0;
    s->first_run = 1;       /* UI-30: first launch shows onboarding splash */
    s->font_family[0] = '\0'; /* INT-15: default family */
    return s;
}

/* process-wide singleton (declared below; forward reference via this static) */
static WubuSettings *g_shared = NULL;

void wubusettings_destroy(WubuSettings *s){
    if (!s) return;
    /* if this is the shared singleton, clear the global so a later
     * wubusettings_shared() re-creates it (don't leave a dangling ptr). */
    if (s == g_shared) g_shared = NULL;
    free(s);
}

static char *expand_path(const char *p){
    /* support a leading ~/ by swapping in $HOME. Caller frees. */
    if (p && p[0]=='~' && (p[1]=='/' || p[1]=='\0')){
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        size_t n = strlen(home) + strlen(p+1) + 1;
        char *out = malloc(n);
        if (!out) return NULL;
        snprintf(out, n, "%s%s", home, p+1);
        return out;
    }
    return p ? strdup(p) : NULL;
}

int wubusettings_load(WubuSettings *s, const char *path){
    if (!s) return -1;
    char *ep = expand_path(path ? path : DEFAULT_PATH);
    if (!ep) return -1;
    FILE *f = fopen(ep, "rb");
    int rc = 0;
    if (!f){
        if (ep != (path?path:DEFAULT_PATH)) free(ep);
        return 0;   /* absent -> keep factory defaults */
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf){ fclose(f); if (ep!=(path?path:DEFAULT_PATH)) free(ep); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd]='\0';
    fclose(f);

    JVal *root = j_parse(buf, NULL);
    free(buf);
    if (root && j_type(root)==J_OBJ){
        const JVal *v;
        if ((v = j_obj_get(root,"zoom")) && j_type(v)==J_NUM) s->zoom = j_as_num(v);
        if ((v = j_obj_get(root,"dark")) && j_type(v)==J_NUM) s->dark = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"autosave_ms")) && j_type(v)==J_NUM) s->autosave_ms = (int)j_as_num(v);
        if ((v = j_obj_get(root,"language")) && j_type(v)==J_STR){
            const char *l = j_as_str(v); strncpy(s->language, l, sizeof s->language-1); s->language[sizeof s->language-1]='\0';
        }
        if ((v = j_obj_get(root,"font_size")) && j_type(v)==J_NUM) s->font_size = (int)j_as_num(v);
        if ((v = j_obj_get(root,"word_wrap")) && j_type(v)==J_NUM) s->word_wrap = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"tab_width")) && j_type(v)==J_NUM){ int tw=(int)j_as_num(v); if(tw<1)tw=1; if(tw>16)tw=16; s->tab_width=tw; }
        if ((v = j_obj_get(root,"high_contrast")) && j_type(v)==J_NUM) s->high_contrast = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"reduced_motion")) && j_type(v)==J_NUM) s->reduced_motion = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"ui_scale")) && j_type(v)==J_NUM){ double us=j_as_num(v); if(us<0.5)us=0.5; if(us>3.0)us=3.0; s->ui_scale=us; }
        if ((v = j_obj_get(root,"first_run")) && j_type(v)==J_NUM) s->first_run = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"font_family")) && j_type(v)==J_STR){
            const char *l = j_as_str(v); strncpy(s->font_family, l, sizeof s->font_family-1); s->font_family[sizeof s->font_family-1]='\0';
        }
        if ((v = j_obj_get(root,"recents")) && j_type(v)==J_ARR){
            s->nrecents = 0;
            for (size_t i=0; i<j_len(v) && s->nrecents<16; i++){
                const JVal *e = j_arr_at(v, i);
                if (e && j_type(e)==J_STR){
                    const char *p = j_as_str(e);
                    snprintf(s->recents[s->nrecents], 256, "%s", p);
                    s->nrecents++;
                }
            }
        }
        rc = 0;
    }
    j_free(root);
    if (ep != (path?path:DEFAULT_PATH)) free(ep);
    return rc;
}

int wubusettings_save(const WubuSettings *s, const char *path){
    if (!s) return -1;
    JVal *root = j_obj();
    if (!root) return -1;
    j_obj_put(root, "zoom", j_num(s->zoom));
    j_obj_put(root, "dark", j_num((double)s->dark));
    j_obj_put(root, "autosave_ms", j_num((double)s->autosave_ms));
    j_obj_put(root, "language", j_str(s->language));
    j_obj_put(root, "font_size", j_num((double)s->font_size));
    j_obj_put(root, "word_wrap", j_num((double)s->word_wrap));
    j_obj_put(root, "tab_width", j_num((double)s->tab_width));
    j_obj_put(root, "high_contrast", j_num((double)s->high_contrast));
    j_obj_put(root, "reduced_motion", j_num((double)s->reduced_motion));
    j_obj_put(root, "ui_scale", j_num(s->ui_scale));
    j_obj_put(root, "first_run", j_num((double)s->first_run));
    j_obj_put(root, "font_family", j_str(s->font_family));
    /* UI-39: recents as a JSON array */
    JVal *arr = j_arr();
    for (int i=0;i<s->nrecents;i++) j_arr_push(arr, j_str(s->recents[i]));
    j_obj_put(root, "recents", arr);
    char *txt = j_emit(root);
    j_free(root);
    if (!txt) return -1;

    char *ep = expand_path(path ? path : DEFAULT_PATH);
    if (!ep){ free(txt); return -1; }
    /* ensure parent dir exists */
    char *slash = strrchr(ep, '/');
    if (slash && slash != ep){ *slash='\0'; mkdir(ep, 0755); *slash='/'; }
    FILE *f = fopen(ep, "wb");
    int rc = -1;
    if (f){ fwrite(txt, 1, strlen(txt), f); fclose(f); rc = 0; }
    free(txt);
    if (ep != (path?path:DEFAULT_PATH)) free(ep);
    return rc;
}

double wubusettings_zoom(const WubuSettings *s){ return s ? s->zoom : 1.0; }
void wubusettings_set_zoom(WubuSettings *s, double z){
    if (!s) return;
    if (z < 0.5) z = 0.5;
    if (z > 3.0) z = 3.0;
    s->zoom = z;
}
int  wubusettings_dark(const WubuSettings *s){ return s ? s->dark : 1; }
void wubusettings_set_dark(WubuSettings *s, int dark){ if (s) s->dark = dark?1:0; }
int  wubusettings_autosave_ms(const WubuSettings *s){ return s ? s->autosave_ms : 5000; }
void wubusettings_set_autosave_ms(WubuSettings *s, int ms){ if (s) s->autosave_ms = ms<0?0:ms; }
const char *wubusettings_language(const WubuSettings *s){ return s ? s->language : "en"; }
void wubusettings_set_language(WubuSettings *s, const char *lang){
    if (!s || !lang) return;
    strncpy(s->language, lang, sizeof s->language-1); s->language[sizeof s->language-1]='\0';
}
int  wubusettings_font_size(const WubuSettings *s){ return s ? s->font_size : 16; }
void wubusettings_set_font_size(WubuSettings *s, int px){ if (s){ if(px<8)px=8; if(px>48)px=48; s->font_size=px; } }

int  wubusettings_word_wrap(const WubuSettings *s){ return s ? s->word_wrap : 1; }
void wubusettings_set_word_wrap(WubuSettings *s, int on){ if (s) s->word_wrap = on?1:0; }
int  wubusettings_tab_width(const WubuSettings *s){ return s ? s->tab_width : 4; }
void wubusettings_set_tab_width(WubuSettings *s, int w){ if (s){ if(w<1)w=1; if(w>16)w=16; s->tab_width=w; } }

int  wubusettings_high_contrast(const WubuSettings *s){ return s ? s->high_contrast : 0; }
void wubusettings_set_high_contrast(WubuSettings *s, int on){ if (s) s->high_contrast = on?1:0; }

/* DOC-43: prefers-reduced-motion (disable animations/transitions). 1 on. */
int  wubusettings_reduced_motion(const WubuSettings *s){ return s ? s->reduced_motion : 0; }
void wubusettings_set_reduced_motion(WubuSettings *s, int on){ if (s) s->reduced_motion = on?1:0; }

/* DOC-45: UI chrome scale, independent of document zoom (1.0 = 100%). */
double wubusettings_ui_scale(const WubuSettings *s){ return s ? s->ui_scale : 1.0; }
void   wubusettings_set_ui_scale(WubuSettings *s, double us){
    if (!s) return;
    if (us < 0.5) us = 0.5;
    if (us > 3.0) us = 3.0;
    s->ui_scale = us;
}

/* UI-30: first-run splash flag. True on a fresh install; the shell clears it
 * (and persists) once the user dismisses the onboarding overlay. */
int  wubusettings_first_run(const WubuSettings *s){ return s ? s->first_run : 1; }
void wubusettings_set_first_run(WubuSettings *s, int on){ if (s) s->first_run = on?1:0; }

/* INT-15: preferred font family (FreeType family_name). */
const char *wubusettings_font_family(const WubuSettings *s){ return s ? s->font_family : ""; }
void wubusettings_set_font_family(WubuSettings *s, const char *family){
    if (!s || !family) return;
    strncpy(s->font_family, family, sizeof s->font_family-1);
    s->font_family[sizeof s->font_family-1]='\0';
}

/* UI-39: recent-documents jump list (most-recent-first, deduped, capped at 16). */
int wubusettings_recents_count(const WubuSettings *s){ return s ? s->nrecents : 0; }
const char *wubusettings_recent(const WubuSettings *s, int i){
    if (!s || i<0 || i>=s->nrecents) return "";
    return s->recents[i];
}
void wubusettings_add_recent(WubuSettings *s, const char *path){
    if (!s || !path || !*path) return;
    /* dedupe: drop any existing copy */
    for (int i=0;i<s->nrecents;i++)
        if (!strcmp(s->recents[i], path)){ s->nrecents--; break; }
    /* shift down to make room at index 0 */
    if (s->nrecents >= 16) s->nrecents = 15;
    for (int i=s->nrecents; i>0; i--)
        snprintf(s->recents[i], 256, "%s", s->recents[i-1]);
    snprintf(s->recents[0], 256, "%s", path);
    s->nrecents++;
}

/* process-wide singleton (defined near top of this file) */
WubuSettings *wubusettings_shared(void){
    if (!g_shared){
        g_shared = wubusettings_create();
        if (g_shared) wubusettings_load(g_shared, NULL);
    }
    return g_shared;
}
