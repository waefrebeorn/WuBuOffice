/* wubuos -- unified WuBuOffice GUI shell (the "whole suite + Notepad++" front
 * end). One SDL2 window hosts every engine behind a WuView adapter selected by
 * a top tab bar. The shell owns the window, tab bar, status bar and scroll, and
 * dispatches events to the active view. Renders are done by the shared
 * wuburender / WuBuPad core / wubucell / wubuocr engines -- no mockups. */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "plugin.h"
#include "settings.h"   /* UI-24/25: zoom + settings persistence */
#include "shape.h"      /* INT-7: RTL shaping (available to views) */
#include "toast.h"      /* UI-33: toast queue */
#include "palette.h"    /* UI-29: command palette */
#include "macro.h"      /* SCR-98: macro record/playback + persistence */
#include "dialog.h"     /* modal text-input dialog (DOC-66/EXP-89/UXA-47/UI-39) */
#include "pasteplain.h" /* EXP-88: paste-plain strips formatting */
#include "wuos_toolbar.h" /* quick-access + formatting toolbar (UI gap) */
#include "wuos_motion.h"  /* easing/tween engine for micro-interactions */
#include "hive.h"         /* data-driven menu/toolbar/slide template */

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WIN_W 960
#define WIN_H 720
/* Chrome heights derive from the base font size (Haiku-DPI pattern, research
 * 2026-08-12): zooming scales the whole UI, not just the view rect. Each is
 * rounded to the 8pt grid (multiple of 4) so all chrome aligns to the spacing
 * scale (Atlassian/USWDS research). At default fh=20 they are 32/24/28/28/224.
 * Note: the tricorder's region coordinates in gui_audit_test.sh must match
 * these defaults (tabs 0-32, menu 32-56, toolbar 56-84, status 692-720). */
#define TAB_H      ((((wuos_font_height()*3)/2) + 2) & ~3)   /* 32 @ fh20 */
#define MENU_H     ((wuos_font_height()+4) & ~3)             /* 24 @ fh20 */
#define TOOLBAR_H  (((wuos_font_height()+6) + 2) & ~3)       /* 28 @ fh20 */
#define SIDEBAR_W  (((wuos_font_height()*11) + 2) & ~3)      /* 224 @ fh20 */
#define STATUS_H   (((wuos_font_height()+6) + 2) & ~3)       /* 28 @ fh20 */

static WuView *views[8];
static int     nviews = 0;
static int     active = 0;
static int     tab_hover = -1;
static int     tab_drag_from = -1;  /* index of tab being dragged, -1 none */
static int     tab_drag_x = 0;      /* pointer x at drag start */
static float   g_zoom = 1.0f;       /* UI-24: shell-level zoom */
static int     g_zoom_drag = 0;     /* zoom slider being dragged */
static int     g_sidebar = 1;       /* docked Navigator panel shown? */
static int     g_ctx = 0;           /* UI-27: context menu open? */
static int     g_ctx_item = 0;      /* highlighted item */
static int     g_ctx_x = 0, g_ctx_y = 0;
static Toasts  *g_toasts = NULL;    /* UI-33: toast queue */
static Palette *g_palette = NULL;   /* UI-29: command palette (Ctrl+K) */

/* ---- micro-interaction / animation state (GUI_EXCELLENCE emotional design,
 * GUI_MATHEMATICS timing & motion). All honored by prefers-reduced-motion. --- */
static WuosTween g_tab_ul;      /* sliding active-tab underline (x0 -> x1) */
static int       g_tab_ul_from = 0, g_tab_ul_to = 0; /* underline span (px) */
static float     g_tb_press_t = 0; /* toolbar button press timestamp (ms) */
static int       g_tb_press_i = -1;/* toolbar button index being pressed */
static Uint32    g_caret_phase = 0;/* caret blink phase (ms) */
static int      g_cheat = 0;       /* UI-36: shortcut cheat-sheet overlay */
static int      g_first_run = 0;    /* UI-30: first-run onboarding splash */
static Dialog  *g_dlg = NULL;       /* modal text-input dialog */
static int      g_dlg_action = 0;   /* 0 none,1 link,2 qr,3 image-alt,10 open,11 save-as */
/* Menu bar + toolbar (UI-43): data-driven from the hive template (hive.json),
 * NOT hardcoded arrays. Selecting an item runs a command id (same space the
 * command palette uses). Pure chrome state here; views stay GUI-free. */
static Hive *g_hive = NULL;         /* loaded at startup (no-hardcoding) */
static const HiveMenu *g_menus = NULL;  /* shorthand for hive_menus() */
static size_t g_nmenus = 0;
static int g_menu_open = -1;       /* which top menu is dropped, -1 none */
static int g_menu_hover = -1;      /* hovered dropdown item */

static int g_tb_hover = -1;        /* hovered toolbar button index (model in
                                    * wuos_toolbar.{h,c}) */

/* Set zoom from a status-bar x position over the zoom slider track. Shared by
 * the click and drag paths so they behave identically. */
static void zoom_from_x(int x){
    int zmin=50, zmax=300, ztrack_x=WIN_W-150, ztrack_w=90;
    double frac = (x - ztrack_x) / (double)ztrack_w;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    g_zoom = (float)(zmin/100.0 + frac*(zmax/100.0 - zmin/100.0));
    if (g_zoom < 0.5f) g_zoom = 0.5f;
    if (g_zoom > 3.0f) g_zoom = 3.0f;
    wuos_font_set_size((int)(20.0 * g_zoom));   /* scale whole UI (Haiku-DPI) */
    WubuSettings *sh=wubusettings_shared(); if(sh) wubusettings_set_zoom(sh, g_zoom);
}

/* Re-apply zoom -> font size after any direct g_zoom change (menu/toolbar/keys). */
static void apply_zoom(void){
    if (g_zoom < 0.5f) g_zoom = 0.5f;
    if (g_zoom > 3.0f) g_zoom = 3.0f;
    wuos_font_set_size((int)(20.0 * g_zoom));
    WubuSettings *sh=wubusettings_shared(); if(sh) wubusettings_set_zoom(sh, g_zoom);
}

static void run_menu_cmd(int cmd);   /* defined below; used by toolbar buttons */

/* Run a toolbar button: menu-space commands via run_menu_cmd, formatting/
 * insert buttons forwarded to the active view's on_key. */
static void run_tb_cmd(int cmd){
    int k = wuos_tb_cmd_to_key(cmd);
    if (k){ if (views[active]->on_key) views[active]->on_key(views[active], k, 1); return; }
    run_menu_cmd(cmd);
}

/* Plugin manager: loaded once at startup from ~/.wubuos/plugins. */
static WuOSPluginMgr *g_plugins = NULL;
static char   *g_plugin_msg = NULL;   /* last exec() result toast */
static int     g_plugin_idx = 0;      /* next plugin to run via Ctrl+Shift+K */

static void add_view(WuView *v){ if (v && nviews<8) views[nviews++]=v; }

/* Run a menu command id (UI-43). Shares the action space with the command
 * palette where possible. `cmd` 1000-1034 are menu-specific. */
static void run_menu_cmd(int cmd){
    switch (cmd){
    case 1000: g_dlg_action = 10; dialog_open(g_dlg, "Open File", "Path:", "");
               toast_push(g_toasts, "Open: type path, Enter", 120); return;
    case 1001: if (views[active]->save) views[active]->save(views[active]);
               toast_push(g_toasts, "Saved", 90); return;
    case 1002: g_dlg_action = 11;
               { const char *cur = (views[active]&&views[active]->get_path)? views[active]->get_path(views[active]):NULL;
                 dialog_open(g_dlg, "Save As", "Path:", cur?cur:""); }
               toast_push(g_toasts, "Save As: type path, Enter", 120); return;
    case 1003: { WuView *nv = wuos_doc_create(NULL);
                 if (nv && nviews<8){ add_view(nv); active=nviews-1; }
                 toast_push(g_toasts, "New document", 90); } return;
    case 1004: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_EPUB, 1);
               toast_push(g_toasts, "EPUB export requested", 120); return;
    case 1030: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_PDF, 1);
               toast_push(g_toasts, "PDF export requested", 120); return;
    case 1031: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_HTML, 1);
               toast_push(g_toasts, "HTML export requested", 120); return;
    case 1032: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_MARKDOWN, 1);
               toast_push(g_toasts, "Markdown export requested", 120); return;
    case 1033: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_LATEX, 1);
               toast_push(g_toasts, "LaTeX export requested", 120); return;
    case 1034: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_RTF, 1);
               toast_push(g_toasts, "RTF export requested", 120); return;
    case 1005: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_UNDO, 1); return;
    case 1006: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_REDO, 1); return;
    case 1007: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_FIND, 1); return;
    case 1008: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_REPLACE, 1); return;
    case 1009: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_GOTO, 1); return;
    case 1010: { WubuSettings *sh=wubusettings_shared(); if (sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                 toast_push(g_toasts, "Theme toggled", 90); } return;
    case 1011: g_zoom += 0.1f; apply_zoom(); toast_push(g_toasts, "Zoom in", 60); return;
    case 1012: g_zoom -= 0.1f; apply_zoom(); toast_push(g_toasts, "Zoom out", 60); return;
    case 1013: g_zoom = 1.0f; apply_zoom(); toast_push(g_toasts, "Zoom reset", 60); return;
    case 1014: { WubuSettings *sh=wubusettings_shared(); if (sh){ wubusettings_set_word_wrap(sh, !wubusettings_word_wrap(sh)); wubusettings_save(sh,NULL);} toast_push(g_toasts, "Word wrap toggled", 90); } return;
    case 1027: /* Close Tab: destroy the active view, fall back to a doc view */
               { WuView *rm = views[active];
                 if (rm && rm->destroy) rm->destroy(rm);
                 if (nviews > 1) { /* shift the rest down */
                   for (int i=active; i+1<nviews; i++) views[i]=views[i+1];
                   nviews--;
                   if (active >= nviews) active = nviews-1;
                 } else { /* last tab: nothing to close but a doc view */
                   toast_push(g_toasts, "No tab to close", 90); return;
                 }
                 toast_push(g_toasts, "Tab closed", 90); }
               return;
    case 1015: { WubuSettings *sh=wubusettings_shared(); if (sh) wubusettings_set_high_contrast(sh, !wubusettings_high_contrast(sh));
                  toast_push(g_toasts, "High contrast toggled", 90); } return;
    case 1016: g_cheat = !g_cheat; toast_push(g_toasts, "Shortcuts", 90); return;
    case 1017: g_first_run = 1; toast_push(g_toasts, "Tour", 90); return;
    case 1018: if (views[active] && views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_CUT, 1); return;
    case 1019: if (views[active] && views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_COPY, 1); return;
    case 1020: SDL_StartTextInput(); return;
    case 1021: if (views[active] && views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_PASTE_PLAIN, 1); return;
    case 1022: if (views[active] && views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_SELECT_ALL, 1); return;
    case 1023: if (views[active]) views[active]->on_key(views[active], WUOS_KEY_EXPORT_PDF, 1); return;
    case 1024: toast_push(g_toasts, "Presentation mode: not yet implemented", 120); return;
    case 1025: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "WuBuOffice",
                 "WuBuOffice v0.1\nA from-scratch office suite with Notepad++ parity.\nBuilt with SDL2 + FreeType + C11.", NULL); return;
    }
}

/* Open `path` in a suitable tab (editor for text-ish, document otherwise),
 * make it active, and record it in the recent-documents list (UI-39). */
static void open_doc_path(const char *path){
    if (!path || !*path) return;
    size_t L = strlen(path);
    int is_slide = (L>4 && (!strcmp(path+L-5,".pptx")||!strcmp(path+L-4,".odp")||
                    !strcmp(path+L-4,".ppt")||!strcmp(path+L-4,".pps")));
    int is_cell  = (L>4 && (!strcmp(path+L-4,".csv")||!strcmp(path+L-5,".xlsx")||
                    !strcmp(path+L-4,".ods")||!strcmp(path+L-5,".xlsm")||
                    !strcmp(path+L-4,".tsv")));
    int is_text = (L>3 && (!strcmp(path+L-3,".md")||!strcmp(path+L-3,".c")||
                  !strcmp(path+L-2,".h")||!strcmp(path+L-3,".py")||
                  !strcmp(path+L-4,".txt")||
                  /* extend editor routing to every plain-text/code format
                   * (depth check: .json/.js/.css/.sql/.cpp/.hpp/.tex/.html
                   *  should open as editable text, not a blank doc view) */
                  !strcmp(path+L-4,".json")||!strcmp(path+L-3,".js")||
                  !strcmp(path+L-4,".css")||!strcmp(path+L-4,".sql")||
                  !strcmp(path+L-4,".cpp")||!strcmp(path+L-4,".hpp")||
                  !strcmp(path+L-4,".tex")||!strcmp(path+L-5,".html")||
                  !strcmp(path+L-4,".xml")||!strcmp(path+L-3,".yml")||
                  !strcmp(path+L-4,".yaml")||!strcmp(path+L-4,".ini")||
                  !strcmp(path+L-3,".sh")||
                  !strcmp(path+L-4,".toml")));
    WuView *nv;
    if (is_slide)     nv = wuos_slide_create(path);  /* presentation view */
    else if (is_cell) nv = wuos_cell_create(path);    /* spreadsheet view */
    else if (is_text) nv = wuos_editor_create(path);
    else              nv = wuos_doc_create(path);
    if (!nv) return;
    if (nviews < 8){ add_view(nv); active = nviews-1; }
    WubuSettings *sh = wubusettings_shared();
    if (sh){ wubusettings_add_recent(sh, path); wubusettings_save(sh, NULL); }
}

static int tab_width(int i);   /* fwd (defined below; uses real font width) */

static int tab_at(int mx){
    int x=0;
    for (int i=0;i<nviews;i++){ int tw = tab_width(i); if (mx>=x && mx<x+tw) return i; x+=tw; }
    return -1;
}

/* Tab width (must stay in sync with the render loop + tab_at). Uses the REAL
 * font advance (11px/char @20, not a hardcoded 14) so tabs don't overlap. */
static int tab_width(int i){ return wuos_font_text_width(views[i]->name, wuos_font_height()) + 24; }

/* Real text width of a label at the current font size (fixed the hardcoded
 * 7px/char estimate that under-sized buttons and made them overlap). */
static int wu_text_w(const char *s){ return wuos_font_text_width(s, wuos_font_height()); }

/* Reorder the tab at `from` to position `to` (indices into views). Updates
 * active to follow the dragged tab. Used by drag-to-reorder. */
static void tab_reorder(int from, int to){
    if (from < 0 || to < 0 || from >= nviews || to >= nviews || from == to) return;
    WuView *mv = views[from];
    if (from < to)
        for (int i=from; i<to; i++) views[i] = views[i+1];
    else
        for (int i=from; i>to; i--) views[i] = views[i-1];
    views[to] = mv;
    if (active == from) active = to;
    else if (from < active && active <= to) active--;
    else if (to <= active && active < from) active++;
}

/* Paint UTF-8 `text` at (px,py) directly onto the SDL renderer using the
 * shared FreeType helper (draws into a 1-line RGBA strip, uploads as texture). */
/* Vertical offset to center chrome text within a band of height `band_h`.
 * sdl_text renders into a surface of height fh+6 (text baseline at fh), so the
 * surface — not just the font height — must be centered in the band. This was
 * the classic "text sits a few px low" misalignment in every chrome row. */
static int center_text_y(int band_h){
    return (band_h - (wuos_font_height() + 6)) / 2;
}

static void sdl_text(SDL_Renderer *ren, int px, int py,
                     unsigned char r, unsigned char g, unsigned char b,
                     const char *text){
    if (!text || !*text) return;
    int fh = wuos_font_height();
    int wpx = wuos_font_text_width(text, fh);   /* measure (draw needs a real fb) */
    if (wpx <= 0) return;
    int W = wpx + 4, H = fh + 6;
    unsigned char *buf = calloc((size_t)W*H*4, 1);
    if (!buf) return;
    for (int y=0;y<H;y++) for (int x=0;x<W;x++){ size_t i=((size_t)y*W+x)*4; buf[i]=0;buf[i+1]=0;buf[i+2]=0;buf[i+3]=0; }
    wuos_font_draw(text, 2, fh, 0, r, g, b, buf, W, H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                        SDL_TEXTUREACCESS_STATIC, W, H);
    if (tex){
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(tex, NULL, buf, W*4);
        SDL_RenderCopy(ren, tex, NULL, &(SDL_Rect){px, py, W, H});
        SDL_DestroyTexture(tex);
    }
    free(buf);
}

int main(int argc, char **argv){
    /* Usage: wubuos [doc|editor|cell|slide|ocr] [file]
     * A bare file path is routed by extension: code/text -> editor,
     * .md/.txt/.html -> document. */
    const char *want_tab = NULL, *want_file = NULL;
    for (int i=1; i<argc; i++){
        if (!want_tab && (!strcmp(argv[i],"doc")||!strcmp(argv[i],"document")||
                          !strcmp(argv[i],"editor")||!strcmp(argv[i],"cell")||
                          !strcmp(argv[i],"slide")||!strcmp(argv[i],"ocr")||
                          !strcmp(argv[i],"compare")))
            want_tab = argv[i];
        else if (!want_file) want_file = argv[i];
    }
    /* extension-based auto-routing for a bare file */
    const char *auto_tab = NULL;
    if (want_file && !want_tab){
        const char *dot = strrchr(want_file, '.');
        if (dot && (!strcasecmp(dot,".md")||!strcasecmp(dot,".txt")||
                    !strcasecmp(dot,".html")||!strcasecmp(dot,".htm")))
            auto_tab = "doc";
        else
            auto_tab = "editor";
    }
    const char *file_for_doc = (want_tab && !strcmp(want_tab,"doc"))? want_file
                              : (auto_tab && !strcmp(auto_tab,"doc"))? want_file : NULL;
    const char *file_for_editor = (want_tab && !strcmp(want_tab,"editor"))? want_file
                                : (auto_tab && !strcmp(auto_tab,"editor"))? want_file : NULL;
    const char *file_for_ocr = (want_tab && (!strcmp(want_tab,"ocr")))? want_file : NULL;
    const char *file_for_cell = (want_tab && !strcmp(want_tab,"cell"))? want_file : NULL;

    if (SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL init: %s\n",SDL_GetError()); return 1; }
    if (wuos_font_init()!=0){ fprintf(stderr,"font init failed\n"); SDL_Quit(); return 1; }

    /* INT-15: restore the user's preferred font family (by name, so it
     * survives index shifts between runs) */
    { WubuSettings *sh = wubusettings_shared();
      const char *pf = sh ? wubusettings_font_family(sh) : "";
      if (pf && *pf){
          for (int i=0;i<wuos_font_family_count();i++)
              if (!strcmp(wuos_font_family_name(i), pf)){ wuos_font_set_family(i); break; }
      } }

    SDL_Window *win = SDL_CreateWindow("WuBuOffice", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){
        /* INT-15: fall back to software renderer for headless/CI (dummy driver,
         * no GPU). The dummy driver only supports SDL_RENDERER_SOFTWARE. */
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    /* HiDPI: map the logical 960x720 canvas onto the device pixel grid so text
     * stays crisp on Retina/4K. Read the display DPI, derive a scale factor
     * relative to the 96-DPI nominal, and let SDL upscale the renderer. */
    float hdpi = 96.0f, vdpi = 96.0f;
    if (SDL_GetDisplayDPI(0, NULL, &hdpi, &vdpi) == 0 && hdpi > 0){
        float s = hdpi / 96.0f;
        if (s < 1.0f) s = 1.0f;          /* never downscale below 1.0 */
        SDL_RenderSetScale(ren, s, s);
    }

    /* load plugins from ~/.wubuos/plugins (if present) */
    g_plugins = wuos_plugins_load(NULL);
    if (g_plugins && wuos_plugins_count(g_plugins)==0)
        fprintf(stderr, "[plugin] no modules in ~/.wubuos/plugins (ok)\n");

    /* load the data-driven hive template: drives the menu bar + toolbar +
     * slide content (no hardcoded UI data). */
    g_hive = hive_load();
    g_menus = hive_menus(g_hive, &g_nmenus);
    wuos_tb_init(hive_toolbar(g_hive));   /* build toolbar from the template */

    add_view(wuos_doc_create(file_for_doc));
    add_view(wuos_cell_create(file_for_cell));
    add_view(wuos_slide_create(NULL));
    add_view(wuos_ocr_create(file_for_ocr));
    add_view(wuos_editor_create(file_for_editor));
    add_view(wuos_compare_create(argc>2?argv[2]:NULL, argc>3?argv[3]:NULL));
    add_view(wuos_settings_create());   /* UI-25: preferences surface */
    if (nviews==0){ fprintf(stderr,"no views\n"); return 1; }

    /* UI-33 toast queue + UI-29 command palette */
    g_toasts = toast_create();
    g_palette = palette_create();
    g_dlg = dialog_create();   /* modal text-input dialog */
    /* UI-30: show first-run splash if this is a fresh install */
    { WubuSettings *sh = wubusettings_shared();
      if (sh && wubusettings_first_run(sh)) g_first_run = 1; }
    /* palette command ids: 1 open-file, 2 new-doc, 3 theme, 4 zoom-in,
     * 5 zoom-out, 6 zoom-reset, 7 settings, 8 export-epub, 9 a11y-check */
    palette_add(g_palette, "New Document",   2);
    palette_add(g_palette, "Toggle Theme",   3);
    palette_add(g_palette, "Zoom In",        4);
    palette_add(g_palette, "Zoom Out",       5);
    palette_add(g_palette, "Zoom Reset",     6);
    palette_add(g_palette, "Open Settings",  7);
    palette_add(g_palette, "Export EPUB",    8);
    palette_add(g_palette, "Accessibility Check", 9);
    palette_add(g_palette, "High Contrast", 10);
 palette_add(g_palette, "Style: Heading 1", 20);
 palette_add(g_palette, "Style: Heading 2", 21);
 palette_add(g_palette, "Style: Heading 3", 22);
 palette_add(g_palette, "Style: Body",      23);
 palette_add(g_palette, "Style: Quote",     24);
 palette_add(g_palette, "Style: Code",      25);
 palette_add(g_palette, "Insert: Script Field", 26);
 palette_add(g_palette, "Macro: Record/Stop", 30);
 palette_add(g_palette, "Macro: Play",        31);
 palette_add(g_palette, "Macro: Save",        32);
 palette_add(g_palette, "Macro: Load",        33);
    /* INT-15: one palette command per enumerated font family */
    for (int fi=0; fi<wuos_font_family_count(); fi++){
        char lbl[96]; snprintf(lbl, sizeof lbl, "Font: %s", wuos_font_family_name(fi));
        palette_add(g_palette, lbl, 100 + fi);
    }
    /* DOC-66 / EXP-89 / UXA-47: modal-dialog prompts (open a Dialog). */
    palette_add(g_palette, "Insert: Hyperlink", 50);
    palette_add(g_palette, "Insert: QR Code",   51);
    palette_add(g_palette, "Insert: Image (alt text)", 52);
    /* UI-39: recent-documents jump list (built from persisted settings). */
    { WubuSettings *sh = wubusettings_shared();
      int nr = sh ? wubusettings_recents_count(sh) : 0;
      for (int i=0; i<nr; i++){
          char lbl[320]; snprintf(lbl, sizeof lbl, "Recent: %s", wubusettings_recent(sh, i));
          palette_add(g_palette, lbl, 200 + i);
      } }

    /* if a specific tab was requested and exists, activate it */
    if (want_tab || auto_tab){
        const char *t = want_tab? want_tab : auto_tab;
        for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name, t) ||
                                      (t && !strcmp(t,"document") && !strcmp(views[i]->name,"Document")) ||
                                      (t && !strcmp(t,"editor") && !strcmp(views[i]->name,"Editor")) ||
                                      (t && !strcmp(t,"compare") && !strcmp(views[i]->name,"Compare"))){ active=i; break; }
    }

    int scroll = 0;
    int running = 1;
    SDL_Event e;

    while (running){
        while (SDL_PollEvent(&e)){
            if (e.type==SDL_QUIT){ running=0; }
            else if (e.type==SDL_MOUSEWHEEL){
                scroll += e.wheel.y*40;
                if (scroll<0) scroll=0;
            }
            else if (e.type==SDL_MOUSEMOTION){
                tab_hover = tab_at(e.motion.x);
                if (e.motion.y < TAB_H){ /* over tab bar: let click switch */ }
                /* toolbar hover (row under the menu bar) */
                g_tb_hover = -1;
                if (e.motion.y >= TAB_H+MENU_H && e.motion.y < TAB_H+MENU_H+TOOLBAR_H)
                    g_tb_hover = wuos_tb_at(e.motion.x);
                /* UI-43: menu-bar hover (top row under the tab strip) */
                g_menu_hover = -1;
                if (e.motion.y >= TAB_H && e.motion.y < TAB_H+MENU_H){
                    int mx=0;
                    for (size_t mi=0; mi<g_nmenus; mi++){
                        int mw = wu_text_w(g_menus[mi].label) + 22;
                        if (e.motion.x>=mx && e.motion.x<mx+mw){ g_menu_hover=(int)mi; break; }
                        mx+=mw;
                    }
                    /* dropdown item hover when a menu is open */
                    if (g_menu_open>=0 && g_menu_hover==g_menu_open){
                        int n = (int)g_menus[g_menu_open].n;
                        int dy = TAB_H + MENU_H, dy0 = e.motion.y - dy - 3;
                        int ii = dy0/24;
                        g_menu_hover = (ii>=0 && ii<n)? (g_menu_open*100+ii) : g_menu_open;
                    }
                }
            }
            else if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                /* UI-43: menu bar (row between TAB_H and TAB_H+MENU_H) */
                if (e.button.y >= TAB_H && e.button.y < TAB_H+MENU_H){
                    int mx=0, hit=-1;
                    for (size_t mi=0; mi<g_nmenus; mi++){
                        int mw = wu_text_w(g_menus[mi].label) + 22;
                        if (e.button.x>=mx && e.button.x<mx+mw){ hit=(int)mi; break; }
                        mx+=mw;
                    }
                    if (hit>=0){
                        /* if a dropdown is open and the click lands on an item, run it */
                        if (g_menu_open>=0 && hit==g_menu_open){
                            int n = (int)g_menus[hit].n;
                            int dy = TAB_H + MENU_H, dy0 = e.button.y - dy - 3;
                            int ii = dy0/24;
                            if (ii>=0 && ii<n && g_menus[hit].items[ii].cmd){
                                int c = g_menus[hit].items[ii].cmd;
                                g_menu_open = -1; g_menu_hover = -1;
                                run_menu_cmd(c);
                                continue;
                            }
                        }
                        g_menu_open = (g_menu_open==hit)? -1 : hit;
                        g_menu_hover = g_menu_open;
                    } else {
                        g_menu_open = -1; g_menu_hover = -1;
                    }
                }
                else if (e.button.y < TAB_H){ int t=tab_at(e.button.x); if(t>=0){ 
                    if (t != active){  /* tab switch: slide the underline (ease-out ~200ms) */
                        int x=0; for (int i=0;i<active;i++) x += tab_width(i);
                        g_tab_ul_from = x; g_tab_ul_to = x + tab_width(active);
                        x=0; for (int i=0;i<t;i++) x += tab_width(i);
                        WubuSettings *msh = wubusettings_shared();
                        int mreduce = msh ? wubusettings_reduced_motion(msh) : 0;
                        wuos_tween_start(&g_tab_ul, (float)g_tab_ul_from, (float)x, 
                                         mreduce ? 0.0f : 0.22f, 1); /* out_quad */
                        g_tab_ul_to = x + tab_width(t);
                    }
                    active=t; scroll=0; tab_drag_from=t; tab_drag_x=e.button.x; } }
                else if (e.button.y >= TAB_H+MENU_H && e.button.y < TAB_H+MENU_H+TOOLBAR_H){
                    /* toolbar button click + press feedback micro-interaction
                     * (research: button active scale 0.98 for ~100ms ease-out). */
                    int ti = wuos_tb_at(e.button.x);
                    if (ti>=0 && wuos_tb_buttons[ti].cmd){
                        g_tb_press_i = ti; g_tb_press_t = (float)SDL_GetTicks();
                        run_tb_cmd(wuos_tb_buttons[ti].cmd);
                    }
                    g_menu_open=-1; g_menu_hover=-1;
                }
                else if (e.button.y >= WIN_H-STATUS_H){ /* status bar: zoom slider */
                    /* click-to-set + begin drag on the track (mirror of render) */
                    int ztrack_x=WIN_W-150, ztrack_w=90;
                    if (e.button.x >= ztrack_x && e.button.x <= ztrack_x+ztrack_w){
                        zoom_from_x(e.button.x);
                        g_zoom_drag = 1;   /* continue adjusting on motion */
                    }
                }
                else if (g_ctx){  /* UI-27: select context-menu item */
                    /* items: 0 Open File, 1 New Document, 2 Toggle Theme */
                    if (g_ctx_item==2){
                        WubuSettings *sh=wubusettings_shared(); if(sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                    }
                    g_ctx = 0;
                }
                else if (views[active]->on_click){  /* clickable links/objects */
                    int lx = e.button.x;
                    int ly = e.button.y - TAB_H - MENU_H - TOOLBAR_H;
                    if (ly >= 0) views[active]->on_click(views[active], lx, ly);
                }
            }
            else if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_RIGHT){
                /* a modal dialog owns input; don't stack a context menu over it */
                if (!dialog_active(g_dlg)){
                    g_ctx = 1; g_ctx_item = 0; g_ctx_x = e.button.x; g_ctx_y = e.button.y;  /* UI-27 */
                }
            }
            else if (e.type==SDL_MOUSEMOTION && g_ctx){
                /* highlight the item under the cursor (3 items, 26px tall) */
                int rel = e.motion.y - g_ctx_y - 4;
                g_ctx_item = (rel>=0)? rel/26 : 0; if (g_ctx_item>2) g_ctx_item=2;
            }
            else if (e.type==SDL_MOUSEMOTION && g_zoom_drag){
                /* drag the zoom slider thumb along the track */
                zoom_from_x(e.motion.x);
            }
            else if (e.type==SDL_MOUSEMOTION && tab_drag_from >= 0){
                /* drag-to-reorder tabs: move the dragged tab to wherever the
                 * pointer now sits (center-based), recomputing live. */
                int target = tab_at(e.motion.x);
                if (target >= 0 && target != tab_drag_from){
                    tab_reorder(tab_drag_from, target);
                    tab_drag_from = target;   /* follow the tab we're dragging */
                }
            }
            else if (e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT){
                g_zoom_drag = 0;   /* release zoom drag */
                tab_drag_from = -1; /* release tab drag */
            }
            else if (e.type==SDL_DROPFILE){   /* UI-28: drag-drop open */
                char *dropped = e.drop.file;
                if (dropped && *dropped) open_doc_path(dropped);  /* records recents */
                SDL_free(dropped);
            }
            else if (e.type==SDL_KEYDOWN){
                SDL_Keycode k = e.key.keysym.sym;
                SDL_Keymod mod = SDL_GetModState();
                int code=0;
                /* UI-30: dismiss the first-run splash on any key */
                if (g_first_run){
                    g_first_run = 0;
                    WubuSettings *sh = wubusettings_shared();
                    if (sh){ wubusettings_set_first_run(sh, 0); wubusettings_save(sh, NULL); }
                    continue;
                }
                /* ---- modal dialog owns input while open (DOC-66/EXP-89/UXA-47) ---- */
                if (dialog_active(g_dlg)){
                    int nk = 0; const char *ch = NULL;
                    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) nk = 13;
                    else if (k == SDLK_ESCAPE) nk = 27;
                    else if (k == SDLK_BACKSPACE) nk = 8;
                    else if (k >= 32 && k < 127){ nk = (int)k; ch = (const char*)&k; }
                    if (nk){
                        int res = dialog_key(g_dlg, nk, ch);
                        if (res == 1){           /* confirmed */
                            const char *txt = dialog_text(g_dlg);
                            WuView *dv = views[active];
                            if (g_dlg_action == 1){ if (wuos_doc_insert_link_url(dv, txt)) toast_push(g_toasts, "Hyperlink inserted", 90); }
                            else if (g_dlg_action == 2){ if (wuos_doc_insert_qr(dv, txt)) toast_push(g_toasts, "QR inserted", 90); }
                            else if (g_dlg_action == 3){ if (wuos_doc_insert_image_alt(dv, txt)) toast_push(g_toasts, "Image inserted", 90); }
                            else if (g_dlg_action == 10){   /* Open file (Ctrl+O) */
                                if (txt && *txt){ open_doc_path(txt); toast_push(g_toasts, "Opened", 90); }
                                else toast_push(g_toasts, "Open: empty path", 120);
                            }
                            else if (g_dlg_action == 11){   /* Save As (Ctrl+Shift+S) */
                                if (dv && dv->set_path && txt && *txt){ dv->set_path(dv, txt); toast_push(g_toasts, "Saved as", 90); }
                                else if (dv && dv->save){ dv->save(dv); toast_push(g_toasts, "Saved", 90); }
                                else toast_push(g_toasts, "Save As: unsupported view", 120);
                            }
                            g_dlg_action = 0;
                        } else if (res == 2){    /* cancelled */
                            g_dlg_action = 0;
                        }
                    }
                    continue;   /* modal: swallow every key while open */
                }
                /* ---- UI-29: command palette captures input while open ---- */
                if (palette_is_open(g_palette)){
                    if (k==SDLK_ESCAPE) palette_close(g_palette);
                    else if (k==SDLK_BACKSPACE) palette_backspace(g_palette);
                    else if (k==SDLK_DOWN) palette_next(g_palette);
                    else if (k==SDLK_UP) palette_prev(g_palette);
                    else if (k==SDLK_RETURN||k==SDLK_KP_ENTER){
                        int cmd = palette_confirm(g_palette);
                        switch (cmd){
                        case 2: { WuView *nv = wuos_doc_create(NULL);
                                  if (nv && nviews<8){ add_view(nv); active=nviews-1; scroll=0; }
                                  toast_push(g_toasts, "New document", 90); } break;
                        case 3: { WubuSettings *sh=wubusettings_shared();
                                  if (sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                                  toast_push(g_toasts, "Theme toggled", 90); } break;
                        case 4: g_zoom += 0.1f; apply_zoom();
                                toast_push(g_toasts, "Zoom in", 60); break;
                        case 5: g_zoom -= 0.1f; apply_zoom();
                                toast_push(g_toasts, "Zoom out", 60); break;
                        case 6: g_zoom = 1.0f; apply_zoom(); toast_push(g_toasts, "Zoom reset", 60); break;
                        case 7: for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; scroll=0; break; }
                                break;
                        case 8: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_EPUB, 1);
                                toast_push(g_toasts, "EPUB export requested", 120); break;
                        case 40: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_PDF, 1);
                                 toast_push(g_toasts, "PDF export requested", 120); break;
                        case 41: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_HTML, 1);
                                 toast_push(g_toasts, "HTML export requested", 120); break;
                        case 42: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_MARKDOWN, 1);
                                 toast_push(g_toasts, "Markdown export requested", 120); break;
                        case 43: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_LATEX, 1);
                                 toast_push(g_toasts, "LaTeX export requested", 120); break;
                        case 44: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_RTF, 1);
                                 toast_push(g_toasts, "RTF export requested", 120); break;
                        case 9: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_A11Y_CHECK, 1);
                                toast_push(g_toasts, "Accessibility check run", 120); break;
                        case 10: { WubuSettings *sh=wubusettings_shared();
                                  if (sh) wubusettings_set_high_contrast(sh, !wubusettings_high_contrast(sh));
                                  toast_push(g_toasts, "High contrast toggled", 90); } break;
                        case 20: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H1, 1);
                                 toast_push(g_toasts, "Style: Heading 1", 90); break;
                        case 21: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H2, 1);
                                 toast_push(g_toasts, "Style: Heading 2", 90); break;
                        case 22: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H3, 1);
                                 toast_push(g_toasts, "Style: Heading 3", 90); break;
                        case 23: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_BODY, 1);
                                 toast_push(g_toasts, "Style: Body", 90); break;
                        case 24: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_QUOTE, 1);
                                 toast_push(g_toasts, "Style: Quote", 90); break;
                        case 25: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_CODE, 1);
                                 toast_push(g_toasts, "Style: Code", 90); break;
                        case 26: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_INSERT_SCRIPT, 1);
                                 toast_push(g_toasts, "Insert: Script Field", 90); break;
                        case 30: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_REC, 1);
                                 toast_push(g_toasts, "Macro: record toggled", 90); break;
                        case 31: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_PLAY, 1);
                                 toast_push(g_toasts, "Macro: play", 90); break;
                        case 32: { const char *mp = getenv("WUBUOS_MACRO_DIR");
                                   char buf[512];
                                   snprintf(buf,sizeof buf,"%s/wubuos_macro.mac", mp? mp : "/tmp");
                                   macro_set_name(macro_create(), "default");
                                   if (macro_save(buf)==0) toast_push(g_toasts, "Macro saved", 90);
                                   else toast_push(g_toasts, "Macro save failed: check WUBUOS_MACRO_DIR is writable", 180); } break;
                        case 33: { const char *mp = getenv("WUBUOS_MACRO_DIR");
                                   char buf[512];
                                   snprintf(buf,sizeof buf,"%s/wubuos_macro.mac", mp? mp : "/tmp");
                                   if (macro_load(buf)==0) toast_push(g_toasts, "Macro loaded", 90);
                                   else toast_push(g_toasts, "Macro load failed: no macro at that path yet", 180); } break;
                        /* DOC-66 / EXP-89 / UXA-47: open the modal dialog. */
                        case 50: g_dlg_action = 1; dialog_open(g_dlg, "Insert Hyperlink", "URL:", "https://"); toast_push(g_toasts, "Hyperlink: type URL, Enter", 120); break;
                        case 51: g_dlg_action = 2; dialog_open(g_dlg, "Insert QR Code", "Text:", ""); toast_push(g_toasts, "QR: type text, Enter", 120); break;
                        case 52: g_dlg_action = 3; dialog_open(g_dlg, "Insert Image", "Alt text:", ""); toast_push(g_toasts, "Image: type alt text, Enter", 120); break;
                        /* UI-39: open a recent document. */
                        default:
                            if (cmd >= 200){
                                int ri = cmd - 200;
                                WubuSettings *sh = wubusettings_shared();
                                if (sh && ri >= 0 && ri < wubusettings_recents_count(sh))
                                    open_doc_path(wubusettings_recent(sh, ri));
                            }
                            else {
                            /* INT-15: font-family commands (id == 100 + index) */
                            if (cmd >= 100){
                                int fi = cmd - 100;
                                if (fi >= 0 && fi < wuos_font_family_count()){
                                    if (wuos_font_set_family(fi)==0){
                                        WubuSettings *sh = wubusettings_shared();
                                        if (sh){ wubusettings_set_font_family(sh, wuos_font_family_name(fi));
                                                 wubusettings_save(sh, NULL); }
                                        toast_push(g_toasts, wuos_font_family_name(fi), 90);
                                    } else toast_push(g_toasts, "Font switch failed: glyphs unavailable for that family", 180);
                                }
                            }
                            }
                            break;
                        }
                    }
                    else if (k>=32 && k<128 && !(mod & KMOD_CTRL)) palette_input(g_palette, (char)k);
                    continue;  /* palette swallows the event */
                }
                if (k==SDLK_k && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)){
                    palette_open(g_palette);   /* UI-29: Ctrl+K */
                    continue;
                }
                if (k==SDLK_ESCAPE){ running=0; }
                else if (k==SDLK_s && (mod & KMOD_CTRL)) code=WUOS_KEY_SAVE;
                else if (k==SDLK_f && (mod & KMOD_CTRL)) code=WUOS_KEY_FIND;
                else if (k==SDLK_h && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACE;
                else if (k==SDLK_r && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACEALL;
                else if (k==SDLK_g && (mod & KMOD_CTRL)) code=WUOS_KEY_GOTO;
                else if (k==SDLK_e && (mod & KMOD_CTRL)) code=WUOS_KEY_EOL;
                else if (k==SDLK_BACKQUOTE && (mod & KMOD_CTRL)) code=WUOS_KEY_THEME;
                else if (k==SDLK_z && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)) code=WUOS_KEY_UNDO;
                else if ((k==SDLK_y && (mod & KMOD_CTRL)) || (k==SDLK_z && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT))) code=WUOS_KEY_REDO;
                else if (k==SDLK_t && (mod & KMOD_CTRL)) code=WUOS_KEY_NEWDOC;
                else if (k==SDLK_w && (mod & KMOD_CTRL)) code=WUOS_KEY_CLOSE;
                else if (k==SDLK_TAB && (mod & KMOD_CTRL))
                    code = (mod & KMOD_SHIFT)? WUOS_KEY_DOCPREV : WUOS_KEY_DOCNEXT;
                else if (k==SDLK_F2 && (mod & KMOD_CTRL)) code=WUOS_KEY_TOGGLE_BK;
                else if (k==SDLK_F2) code=(mod & KMOD_SHIFT)? WUOS_KEY_PREV_BK : WUOS_KEY_NEXT_BK;
                else if (k==SDLK_c && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_COLMODE;
                else if (k==SDLK_r && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_REC;
                else if (k==SDLK_p && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PLAY;
                else if (k==SDLK_SPACE && (mod & KMOD_CTRL)) code=WUOS_KEY_AC;
                else if (k==SDLK_s && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_SESSION;
                else if (k==SDLK_b && (mod & KMOD_CTRL)) code=WUOS_KEY_SIDEBAR;   /* Ctrl+B: toggle Navigator sidebar */
                else if (k==SDLK_o && (mod & KMOD_CTRL)){   /* Ctrl+O: Open file dialog */
                    g_dlg_action = 10;
                    const char *cur = (views[active] && views[active]->get_path) ? views[active]->get_path(views[active]) : NULL;
                    dialog_open(g_dlg, "Open File", "Path:", cur ? cur : "");
                    toast_push(g_toasts, "Open: type path, Enter", 120);
                    continue;
                }
                else if (k==SDLK_a && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)){   /* Ctrl+Shift+A: Save As dialog */
                    g_dlg_action = 11;
                    const char *cur = (views[active] && views[active]->get_path) ? views[active]->get_path(views[active]) : NULL;
                    dialog_open(g_dlg, "Save As", "Path:", cur ? cur : "");
                    toast_push(g_toasts, "Save As: type path, Enter", 120);
                    continue;
                }
                else if (k==SDLK_f && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_FOLD;
                else if (k==SDLK_l && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_FUNCLIST;
                else if (k==SDLK_k && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PLUGIN;
                else if (k==SDLK_F10) code=WUOS_KEY_SETTINGS;
                else if ((k==SDLK_EQUALS || k==SDLK_PLUS) && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_IN;
                else if (k==SDLK_MINUS && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_OUT;
                else if (k==SDLK_0 && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_RESET;
                else if (k==SDLK_l && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_LINK; /* DOC-60 */
                else if (k==SDLK_l && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_LIST; /* DOC-59 */
                else if (k==SDLK_t && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_TABLE; /* DOC-62 */
                else if (k==SDLK_i && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_IMAGE; /* DOC-61 */
                else if (k==SDLK_b && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_PAGEBREAK; /* DOC-57 */
                else if (k==SDLK_s && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_SECTIONBREAK; /* DOC-57 */
                else if (k==SDLK_h && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_HEADER; /* DOC-56 */
                else if (k==SDLK_f && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_FOOTER; /* DOC-56 */
                else if (k==SDLK_c && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_COMMENT; /* DOC-63 */
                else if (k==SDLK_t && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT) && (mod & KMOD_ALT)) code=WUOS_KEY_INSERT_TRACKCHANGE; /* DOC-64 */
                else if (k==SDLK_d && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_FIELD; /* DOC-65 */
                else if (k==SDLK_g && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_SCRIPT; /* DOC-97 */
                else if (k==SDLK_1 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H1; /* DOC-58 */
                else if (k==SDLK_2 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H2; /* DOC-58 */
                else if (k==SDLK_3 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H3; /* DOC-58 */
                else if (k==SDLK_UP && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PARA_PREV; /* DOC-58 */
                else if (k==SDLK_DOWN && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PARA_NEXT; /* DOC-58 */
                else if (k==SDLK_F1) code=WUOS_KEY_CHEAT;   /* UI-36 */
                else if (k>=SDLK_1 && k<=SDLK_6 && (mod & KMOD_CTRL))
                    code = WUOS_KEY_TOC1 + (k - SDLK_1);   /* DOC-54 jump */
                else if (k==SDLK_F3) code=(mod & KMOD_SHIFT)? WUOS_KEY_FINDPREV : WUOS_KEY_FINDNEXT;
                else if (k==SDLK_UP) code=WUOS_KEY_UP;
                else if (k==SDLK_DOWN) code=WUOS_KEY_DOWN;
                else if (k==SDLK_LEFT) code=WUOS_KEY_LEFT;
                else if (k==SDLK_RIGHT) code=WUOS_KEY_RIGHT;
                else if (k==SDLK_BACKSPACE) code=WUOS_KEY_BACKSPACE;
                else if (k==SDLK_RETURN||k==SDLK_KP_ENTER) code=WUOS_KEY_RETURN;
                else if (k==SDLK_TAB) code=WUOS_KEY_TAB;
                else if (k==SDLK_HOME) code=WUOS_KEY_HOME;
                else if (k==SDLK_END) code=WUOS_KEY_END;
                else if (k==SDLK_PAGEUP) code=WUOS_KEY_PGUP;
                else if (k==SDLK_PAGEDOWN) code=WUOS_KEY_PGDN;
                else if (k==SDLK_DELETE) code=WUOS_KEY_DEL;
                else if (k==SDLK_x && (mod&KMOD_CTRL)) code=WUOS_KEY_CUT;
                else if (k==SDLK_c && (mod&KMOD_CTRL)) code=WUOS_KEY_COPY;
                else if (k==SDLK_v && (mod&KMOD_CTRL) && !(mod&KMOD_SHIFT)) code=WUOS_KEY_PASTE;
                else if (k==SDLK_v && (mod&KMOD_CTRL) && (mod&KMOD_SHIFT)) code=WUOS_KEY_PASTE_PLAIN;
                else if (k==SDLK_a && (mod&KMOD_CTRL)) code=WUOS_KEY_SELECT_ALL;
                else if (k==SDLK_w && (mod&KMOD_CTRL)) code=WUOS_KEY_CLOSE;
                else if (k==SDLK_F5) code=WUOS_KEY_PRESENTATION;
                else if (k>=32 && k<128) code=(int)k;
                if (code && views[active]->on_key) views[active]->on_key(views[active], code, 1);

                /* plugin action: Ctrl+Shift+K runs the next loaded plugin */
                if (code == WUOS_KEY_PLUGIN){
                    if (!g_plugins || wuos_plugins_count(g_plugins)==0){
                        free(g_plugin_msg);
                        g_plugin_msg = strdup("no plugins loaded");
                    } else {
                        char *r = wuos_plugins_exec(g_plugins, g_plugin_idx, NULL);
                        free(g_plugin_msg);
                        g_plugin_msg = r;
                        g_plugin_idx = (g_plugin_idx + 1) % wuos_plugins_count(g_plugins);
                    }
                }

                /* EXP-88 paste-plain: Ctrl+Shift+V strips formatting */
                else if (code == WUOS_KEY_PASTE_PLAIN){
                    char *clip = SDL_GetClipboardText();
                    if (clip){
                        char *plain = pasteplain_strip(clip);
                        if (plain && views[active] && views[active]->on_key){
                            for (char *p = plain; *p; p++){
                                views[active]->on_key(views[active], (int)*p, 1);
                            }
                        }
                        free(plain);
                        SDL_free(clip);
                    }
                }

                /* ---- shell-level features (not forwarded to the view) ---- */
                if (code == WUOS_KEY_ZOOM_IN){
                    g_zoom += 0.1f; apply_zoom();
                } else if (code == WUOS_KEY_ZOOM_OUT){
                    g_zoom -= 0.1f; apply_zoom();
                } else if (code == WUOS_KEY_ZOOM_RESET){
                    g_zoom = 1.0f; apply_zoom();
                } else if (code == WUOS_KEY_SETTINGS){
                    for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; scroll=0; break; }
                } else if (code == WUOS_KEY_SIDEBAR){
                    g_sidebar = !g_sidebar;   /* toggle Navigator sidebar */
                } else if (code == WUOS_KEY_CHEAT){
                    g_cheat = !g_cheat;   /* UI-36 toggle */
                }
            }
        }

        /* WUOS_DUMP_VIEW: switch to the named view BEFORE the first render so
         * the dump captures that view (not the default Document). */
        {
            static int view_switched = 0;
            if (!view_switched && getenv("WUOS_DUMP_VIEW")){
                const char *dv = getenv("WUOS_DUMP_VIEW");
                for (int i=0;i<nviews;i++)
                    if (views[i]->name && !strcmp(views[i]->name, dv)){ active=i; scroll=0; break; }
                view_switched = 1;
            }
        }
        /* render active view (placed below the tab strip, menu bar AND toolbar) */
        int view_top = TAB_H + MENU_H + TOOLBAR_H;
        int sb_w = (g_sidebar && views[active]->sidebar) ? SIDEBAR_W : 0;  /* dock sidebar */
        unsigned char *rgba=NULL; int rw=0, rh=0;
        if (views[active]->render(views[active], WIN_W - sb_w, WIN_H - view_top - STATUS_H, scroll, &rgba, &rw, &rh)!=0)
            rgba=NULL;

        SDL_SetRenderDrawColor(ren, 235,237,240,255);
        SDL_RenderClear(ren);
        if (rgba){
            SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, rw, rh);
            if (tex){
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
                SDL_UpdateTexture(tex, NULL, rgba, rw*4);
                int maxscroll = (rh > (WIN_H-view_top-STATUS_H))? rh-(WIN_H-view_top-STATUS_H):0;
                if (scroll>maxscroll) scroll=maxscroll;
                /* UI-24: apply shell zoom by scaling the destination rect */
                int draw_w = (int)(rw * g_zoom);
                int draw_h = (int)(rh * g_zoom);
                SDL_Rect src={0,scroll,rw,rh}; SDL_Rect dst={0,view_top,draw_w,draw_h};
                if (draw_h < (WIN_H-view_top-STATUS_H)){ dst.y=view_top; dst.h=draw_h; src.h=rh; }
                else { src.h=(WIN_H-view_top-STATUS_H); dst.h=(WIN_H-view_top-STATUS_H); }
                SDL_RenderCopy(ren, tex, &src, &dst);
                SDL_DestroyTexture(tex);
                free(rgba);
            } else free(rgba);
        }

        /* tab bar */
        int x=0;
        int dark = wubusettings_dark(wubusettings_shared());
        /* Tab bar: neutral chrome ground; each tab a quiet segment; the ACTIVE
         * tab rises to a lighter surface + a 2px accent underline. No bright
         * blue fill (spec §5). Inactive tabs dim. (Tokens: wuos_theme.h.) */
        WuosRGB tbb = dark ? (WuosRGB)WUOS_DARK_TAB_BAR : (WuosRGB)WUOS_LIGHT_TAB_BAR;
        	WuosRGB ttk = dark ? (WuosRGB)WUOS_DARK_TAB     : (WuosRGB)WUOS_LIGHT_TAB;
        WuosRGB tto = dark ? (WuosRGB)WUOS_DARK_TAB_ON  : (WuosRGB)WUOS_LIGHT_TAB_ON;
        WuosRGB ttx = dark ? (WuosRGB)WUOS_DARK_TABTEXT : (WuosRGB)WUOS_LIGHT_TABTEXT;
        WuosRGB ttxo= dark ? (WuosRGB)WUOS_DARK_TABTEXT_ON:(WuosRGB)WUOS_LIGHT_TABTEXT_ON;
        WuosRGB bd  = dark ? (WuosRGB)WUOS_DARK_BORDER  : (WuosRGB)WUOS_LIGHT_BORDER;
        WuosRGB ac  = dark ? (WuosRGB)WUOS_DARK_ACCENT  : (WuosRGB)WUOS_LIGHT_ACCENT;
        SDL_SetRenderDrawColor(ren, tbb.r, tbb.g, tbb.b, 255);
        SDL_RenderFillRect(ren,&(SDL_Rect){0,0,WIN_W,TAB_H});
        for (int i=0;i<nviews;i++){
            int tw = tab_width(i);
            int on = (i==active);
            int hov = (i==tab_hover);
            int dragging = (i==tab_drag_from);
            WuosRGB seg = on ? tto : (hov ? tto : ttk);
            SDL_SetRenderDrawColor(ren, seg.r, seg.g, seg.b, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x,0,tw,TAB_H});
            /* 1px right divider */
            SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x+tw-1,0,1,TAB_H});
            /* dragged tab: 2px accent overline so the user sees it's in motion */
            if (dragging){
                SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, 255);
                SDL_RenderFillRect(ren,&(SDL_Rect){x,0,tw,2});
            }
            sdl_text(ren, x+12, center_text_y(TAB_H),
                     on?ttxo.r:ttx.r, on?ttxo.g:ttx.g, on?ttxo.b:ttx.b,
                     views[i]->name);
            x+=tw;
        }
        /* Active-tab underline SLIDES to the new active tab (research:
         * motion.dev/ui-layouts smooth-tabs: a sliding indicator). Drawn once,
         * after the loop. When idle it sits on the active tab; during a tab
         * switch the tween eases the left edge across (out_quad ~200ms).
         * prefers-reduced-motion -> tween done instantly (dur 0). */
        {
            WubuSettings *ash = wubusettings_shared();
            int areduce = ash ? wubusettings_reduced_motion(ash) : 0;
            float ul_x, ul_w;
            if (areduce || wuos_tween_done(&g_tab_ul)){
                int xa = 0; for (int i=0;i<active;i++) xa += tab_width(i);
                ul_x = (float)xa; ul_w = (float)tab_width(active);
            } else {
                ul_x = wuos_tween_value(&g_tab_ul);
                ul_w = (float)g_tab_ul_to - ul_x;
                if (ul_w < 2.0f) ul_w = 2.0f;
            }
            SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){(int)ul_x, TAB_H-2, (int)ul_w, 2});
        }
        /* menu bar (UI-43): second chrome row below the tab strip. Top-level
         * items; the open one drops a command list. Neutral surface, accent
         * underline on hover/active (matches the tab-bar language). */
        {
            int my = TAB_H;
            SDL_SetRenderDrawColor(ren, tbb.r, tbb.g, tbb.b, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){0, my, WIN_W, MENU_H});
            int mx = 0;
            for (size_t mi=0; mi<g_nmenus; mi++){
                int mw = wu_text_w(g_menus[mi].label) + 22;
                int on = ((int)mi==g_menu_open);
                int hovered = (g_menu_hover==(int)mi || (g_menu_open==(int)mi && g_menu_hover>=(int)mi*100 && g_menu_hover<(int)mi*100+100));
                if (on || hovered){
                    SDL_SetRenderDrawColor(ren, tto.r, tto.g, tto.b, 255);
                    SDL_RenderFillRect(ren, &(SDL_Rect){mx, my, mw, MENU_H});
                }
                sdl_text(ren, mx+11, my + center_text_y(MENU_H),
                         (on||hovered)?ttxo.r:ttx.r, (on||hovered)?ttxo.g:ttx.g, (on||hovered)?ttxo.b:ttx.b,
                         g_menus[mi].label);
                if (on){
                    /* dropdown: widen to fit the longest item + accelerator */
                    int n = (int)g_menus[mi].n;
                    int dw = mw;
                    for (int k=0; k<n; k++){
                        if (!g_menus[mi].items[k].label) continue;
                        int w = wu_text_w(g_menus[mi].items[k].label) + 12;
                        const char *a = g_menus[mi].items[k].accel;
                        if (a && *a) w += wu_text_w(a) + 14;
                        if (w > dw) dw = w;
                    }
                    /* dropdown */
                    int dy = my + MENU_H;
                    int dh = n*24 + 6;
                    SDL_SetRenderDrawColor(ren, tto.r, tto.g, tto.b, 255);
                    SDL_RenderFillRect(ren, &(SDL_Rect){mx, dy, dw, dh});
                    SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
                    SDL_RenderDrawRect(ren, &(SDL_Rect){mx, dy, dw, dh});
                    for (int i=0;i<n;i++){
                        int iy = dy + 3 + i*24;
                        int item_hover = (g_menu_hover==(int)mi*100+i);
                        if (!g_menus[mi].items[i].label) continue;  /* separator */
                        if (item_hover){
                            SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, 255);
                            SDL_RenderFillRect(ren, &(SDL_Rect){mx+1, iy, mw-2, 20});
                        }
                        sdl_text(ren, mx+8, iy + center_text_y(24),
                                 item_hover?ttxo.r:ttx.r,
                                 item_hover?ttxo.g:ttx.g,
                                 item_hover?ttxo.b:ttx.b,
                                 g_menus[mi].items[i].label);
                        /* right-aligned accelerator hint (discoverability).
                         * Right-align within the DROPDOWN width (dw), not the
                         * top menu width (mw) — mw is narrower so the hint
                         * drifted off into the middle. Full brightness: the
                         * old *0.7 dim was unreadable in dark mode. */
                        const char *acc = g_menus[mi].items[i].accel;
                        if (acc && *acc){
                            int alen = wu_text_w(acc);
                            sdl_text(ren, mx + dw - 10 - alen,
                                     iy + center_text_y(24),
                                     ttxo.r, ttxo.g, ttxo.b, acc);
                        }
                    }
                }
                mx += mw;
            }
        }
        /* toolbar row (below the menu bar): quick-access + formatting buttons.
         * Neutral surface, quiet buttons; hovered/clicked rise to the accent
         * tint; separators are thin vertical rules. Same token language as the
         * tab + menu chrome. */
        {
            int ty = TAB_H + MENU_H;
            SDL_SetRenderDrawColor(ren, tbb.r, tbb.g, tbb.b, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){0, ty, WIN_W, TOOLBAR_H});
            /* resting button surface: slightly raised from the bar bg so each
             * button reads as a distinct clickable target (affordance).
             * Derived from the theme bar color. ~5% lift (not 14%): brightening
             * too much dropped the toolbar TEXT contrast below WCAG AA 4.5:1
             * (measured 3.70:1 @14%); 5% keeps the affordance while text stays
             * >=4.5:1. */
            WuosRGB tbo = { (unsigned char)(tbb.r + (255-tbb.r)*5/100),
                            (unsigned char)(tbb.g + (255-tbb.g)*5/100),
                            (unsigned char)(tbb.b + (255-tbb.b)*5/100) };
            int tx = 0;
            for (size_t i=0;i<wuos_tb_count;i++){
                const WuosTbBtn *b = &wuos_tb_buttons[i];
                if (!b->label){ /* separator rule */
                    SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
                    SDL_RenderFillRect(ren, &(SDL_Rect){tx, ty+5, 1, TOOLBAR_H-10});
                    tx += 10;
                    continue;
                }
                int bw = wu_text_w(b->label) + 10;   /* REAL text width + pad */
                int hov = (g_tb_hover==(int)i);
                /* press micro-interaction: button briefly deepens for ~100ms
                 * (research: active state scale/color feedback, ease-out). */
                int press = 0;
                if (g_tb_press_i == (int)i){
                    float age = (float)SDL_GetTicks() - g_tb_press_t;
                    if (age < 100.0f) press = 1;
                    else g_tb_press_i = -1;
                }
                if (hov || press){
                    /* pressed = slightly deeper/more opaque than hover */
                    SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, press ? 255 : 235);
                    SDL_RenderFillRect(ren, &(SDL_Rect){tx, ty+3, bw, TOOLBAR_H-6});
                } else {
                    /* resting-state button affordance: a subtle raised surface so
                     * buttons read as distinct targets, not one merged string
                     * (research: button = visible container, not bare text). */
                    SDL_SetRenderDrawColor(ren, tbo.r, tbo.g, tbo.b, 255);
                    SDL_RenderFillRect(ren, &(SDL_Rect){tx+1, ty+5, bw-2, TOOLBAR_H-10});
                }
                sdl_text(ren, tx + (bw - wu_text_w(b->label))/2,
                         ty + center_text_y(TOOLBAR_H),
                         (hov||press) ? ttxo.r : ttx.r, (hov||press) ? ttxo.g : ttx.g, (hov||press) ? ttxo.b : ttx.b,
                         b->label);
                tx += bw + 1;
            }
            /* bottom divider */
            SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){0, ty+TOOLBAR_H-1, WIN_W, 1});
        }
        /* docked Navigator sidebar (right). Reference office apps have a
         * collapsible sidebar (LibreOffice/OnlyOffice/MS Office); the shell
         * docks a right panel whose content is the active view's real
         * structure (doc TOC / editor functions / cell values). */
        if (g_sidebar){
            char *sb = (views[active]->sidebar) ? views[active]->sidebar(views[active]) : NULL;
            /* A NULL or empty result still renders a GUIDED panel (an empty
             * pane must not be a blank surface — GUI_EXCELLENCE paradigm 8). */
            char *empty = NULL;
            if (!sb){ empty = ""; sb = empty; }
            {
                int sx = WIN_W - SIDEBAR_W, sy = view_top;
                int sh = WIN_H - view_top - STATUS_H;
                WuosRGB sbbg = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
                WuosRGB sbbd = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
                WuosRGB sbtx = dark ? WUOS_DARK(OVERLAY_TEXT)   : WUOS_LIGHT(OVERLAY_TEXT);
                SDL_SetRenderDrawColor(ren, sbbg.r, sbbg.g, sbbg.b, 255);
                SDL_RenderFillRect(ren, &(SDL_Rect){sx, sy, SIDEBAR_W, sh});
                SDL_SetRenderDrawColor(ren, sbbd.r, sbbd.g, sbbd.b, 255);
                SDL_RenderFillRect(ren, &(SDL_Rect){sx, sy, 1, sh});
                /* header */
                sdl_text(ren, sx+10, sy+6, sbtx.r, sbtx.g, sbtx.b, "Navigator");
                SDL_SetRenderDrawColor(ren, sbbd.r, sbbd.g, sbbd.b, 255);
                SDL_RenderFillRect(ren, &(SDL_Rect){sx, sy+24, SIDEBAR_W, 1});
                /* entries (split lines) */
                int iy = sy + 32, lh = 16;
                if (!*sb){   /* guided empty state (GUI_EXCELLENCE paradigm 8):
                             * an empty panel must guide, not be blank. */
                    sdl_text(ren, sx+8, iy, sbtx.r, sbtx.g, sbtx.b,
                             "No structure yet");
                    iy += lh;
                    sdl_text(ren, sx+8, iy, sbtx.r, sbtx.g, sbtx.b,
                             "This view has no outline");
                }
                char *line = sb, *nl;
                while (line && *line && iy < sy+sh-4){
                    nl = strchr(line, '\n');
                    int L = nl ? (int)(nl - line) : (int)strlen(line);
                    if (L > 0){
                        char buf[96]; if (L>95) L=95;
                        memcpy(buf, line, L); buf[L]=0;
                        sdl_text(ren, sx+8, iy, sbtx.r, sbtx.g, sbtx.b, buf);
                        iy += lh;
                    }
                    line = nl ? nl+1 : NULL;
                }
                if (empty) sb = NULL; /* don't free the literal "" */
                if (sb) free(sb);
            }
        }
        /* status bar */
        char *st = views[active]->status? views[active]->status(views[active]) : NULL;
        WuosRGB sb = dark ? (WuosRGB)WUOS_DARK_STATUS : (WuosRGB)WUOS_LIGHT_STATUS;
        WuosRGB stx= dark ? (WuosRGB)WUOS_DARK_STATUSTX: (WuosRGB)WUOS_LIGHT_STATUSTX;
        SDL_SetRenderDrawColor(ren, sb.r, sb.g, sb.b, 255);
        SDL_RenderFillRect(ren,&(SDL_Rect){0,WIN_H-STATUS_H,WIN_W,STATUS_H});
        /* 1px top divider on the status bar */
        SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
        SDL_RenderFillRect(ren,&(SDL_Rect){0,WIN_H-STATUS_H,WIN_W,1});
        if (st){
            sdl_text(ren, 12, WIN_H-STATUS_H + center_text_y(STATUS_H),
                     stx.r, stx.g, stx.b, st);
            free(st);
        }

        /* plugin toast (Ctrl+Shift+K result) */
        if (g_plugin_msg){
            sdl_text(ren, WIN_W-360, WIN_H-STATUS_H + center_text_y(STATUS_H),
                     120,220,140, g_plugin_msg);
        }

        /* zoom indicator + slider (right of the status bar). Reference office
         * apps (LibreOffice/OnlyOffice/MS Office/Notepad++) all show zoom in the
         * status bar; the shell previously exposed zoom only via keyboard/menu.
         * Slider: a thin track [MIN..MAX]; thumb at current g_zoom. Clicking the
         * track re-positions zoom (handled in the mouse handler). */
        {
            int zmin = 50, zmax = 300;
            int ztxt_x = WIN_W - 210, ztrack_x = WIN_W - 150, ztrack_w = 90;
            int zy = WIN_H - STATUS_H;
            char zlabel[16];
            snprintf(zlabel, sizeof zlabel, "%d%%", (int)(g_zoom*100));
            sdl_text(ren, ztxt_x, zy + center_text_y(STATUS_H),
                     stx.r, stx.g, stx.b, zlabel);
            /* track */
            SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){ztrack_x, zy+STATUS_H/2-1, ztrack_w, 2});
            /* fill to current */
            SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, 255);
            int fill = (int)((g_zoom - zmin/100.0) / (zmax/100.0 - zmin/100.0) * ztrack_w);
            if (fill > ztrack_w) fill = ztrack_w;
            if (fill < 0) fill = 0;
            SDL_RenderFillRect(ren, &(SDL_Rect){ztrack_x, zy+STATUS_H/2-1, fill, 2});
            /* thumb */
            SDL_SetRenderDrawColor(ren, stx.r, stx.g, stx.b, 255);
            SDL_RenderFillRect(ren, &(SDL_Rect){ztrack_x+fill-2, zy+STATUS_H/2-4, 5, 8});
        }

        /* UI-27: right-click context menu overlay */
        if (g_ctx){
            const char *items[3] = { "Open File…", "New Document", "Toggle Theme" };
            int mw = WUOS_SPACE_4 * 40, mh = WUOS_SPACE_4 * 20;
            int mx = g_ctx_x, my = g_ctx_y;
            if (mx+mw > WIN_W) mx = WIN_W-mw;
            if (my+mh > WIN_H-STATUS_H) my = WIN_H-STATUS_H-mh;
            WuosRGB ctx_bg = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
            WuosRGB ctx_bd = dark ? WUOS_DARK(OVERLAY_BD)   : WUOS_LIGHT(OVERLAY_BD);
            WuosRGB ctx_hl = dark ? WUOS_DARK(OVERLAY_HIGHLIGHT) : WUOS_LIGHT(OVERLAY_HIGHLIGHT);
            WuosRGB ctx_txt = dark ? WUOS_DARK(OVERLAY_TEXT)    : WUOS_LIGHT(OVERLAY_TEXT);
            SDL_SetRenderDrawColor(ren, ctx_bg.r, ctx_bg.g, ctx_bg.b, 255); SDL_RenderFillRect(ren,&(SDL_Rect){mx,my,mw,mh});
            SDL_SetRenderDrawColor(ren, ctx_bd.r, ctx_bd.g, ctx_bd.b, 255); SDL_RenderDrawRect(ren,&(SDL_Rect){mx,my,mw,mh});
            for (int i=0;i<3;i++){
                if (i==g_ctx_item){ SDL_SetRenderDrawColor(ren,ctx_hl.r,ctx_hl.g,ctx_hl.b,255); SDL_RenderFillRect(ren,&(SDL_Rect){mx+WUOS_SPACE_2,my+WUOS_SPACE_4+i*WUOS_SPACE_26,mw-WUOS_SPACE_4,WUOS_SPACE_24}); }
                sdl_text(ren, mx+WUOS_SPACE_4, my+WUOS_SPACE_8+i*WUOS_SPACE_26, ctx_txt.r,ctx_txt.g,ctx_txt.b, items[i]);
            }
        }

        /* modal text-input dialog overlay (DOC-66/EXP-89/UXA-47) */
        if (dialog_active(g_dlg)){
            int bw = WUOS_SPACE_32*16 + WUOS_SPACE_8*2, bx = (WIN_W-bw)/2, by = TAB_H + WUOS_SPACE_16, bh = WUOS_SPACE_24*5;
            WuosRGB dlg_bg = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
            WuosRGB dlg_bd = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
            WuosRGB dlg_ttl = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);
            WuosRGB dlg_pmt = dark ? WUOS_DARK(OVERLAY_HINTS) : WUOS_LIGHT(OVERLAY_HINTS);
            WuosRGB dlg_txt = dark ? WUOS_DARK(OVERLAY_TEXT)    : WUOS_LIGHT(OVERLAY_TEXT);
            WuosRGB dlg_crt = dark ? WUOS_DARK(TABTEXT_ON)       : WUOS_LIGHT(TABTEXT_ON);
            WuosRGB dlg_hnt = dark ? WUOS_DARK(OVERLAY_HINTS)    : WUOS_LIGHT(OVERLAY_HINTS);
            SDL_SetRenderDrawColor(ren, dlg_bg.r, dlg_bg.g, dlg_bg.b, 252); SDL_RenderFillRect(ren,&(SDL_Rect){bx,by,bw,bh});
            SDL_SetRenderDrawColor(ren, dlg_bd.r, dlg_bd.g, dlg_bd.b, 255); SDL_RenderDrawRect(ren,&(SDL_Rect){bx,by,bw,bh});
            sdl_text(ren, bx+WUOS_SPACE_8, by+WUOS_SPACE_4, dlg_ttl.r,dlg_ttl.g,dlg_ttl.b, dialog_title(g_dlg));
            sdl_text(ren, bx+WUOS_SPACE_8, by+WUOS_SPACE_16, dlg_pmt.r,dlg_pmt.g,dlg_pmt.b, dialog_prompt(g_dlg));
            sdl_text(ren, bx+WUOS_SPACE_8, by+WUOS_SPACE_24, dlg_txt.r,dlg_txt.g,dlg_txt.b, dialog_text(g_dlg));
            /* caret (blinks ~530ms, the standard rate; honor reduced-motion) */
            WubuSettings *crsh = wubusettings_shared();
            int crreduce = crsh ? wubusettings_reduced_motion(crsh) : 0;
            int blink_on = crreduce || ((g_caret_phase / 530) % 2 == 0);
            int cw = wuos_font_text_width(dialog_text(g_dlg), 20);
            if (blink_on){
                SDL_SetRenderDrawColor(ren, dlg_crt.r, dlg_crt.g, dlg_crt.b, 255);
                SDL_RenderFillRect(ren,&(SDL_Rect){bx+WUOS_SPACE_8+cw, by+WUOS_SPACE_20, 2, WUOS_SPACE_20});
            }
            sdl_text(ren, bx+WUOS_SPACE_8, by+bh-WUOS_SPACE_4, dlg_hnt.r,dlg_hnt.g,dlg_hnt.b, "Enter to confirm \xe2\x80\xa2 Esc to cancel");
        }

        /* UI-33: toast overlay (bottom-center) + tick */
        toast_tick(g_toasts);
        const char *tt = toast_text(g_toasts);
        if (tt){
            /* DOC-43: prefers-reduced-motion -> no fade-in (instant full opacity) */
            static const char *g_last_tt = NULL;
            static float g_toast_a = 0.0f;
            if (tt != g_last_tt){ g_last_tt = tt; g_toast_a = 0.0f; }
            WubuSettings *sh = wubusettings_shared();
            int reduce = sh ? wubusettings_reduced_motion(sh) : 0;
            if (!reduce && g_toast_a < 1.0f) g_toast_a += 0.12f;
            if (g_toast_a > 1.0f) g_toast_a = 1.0f;
            float ta = reduce ? 1.0f : g_toast_a;
            int tw = (int)strlen(tt)*8 + WUOS_SPACE_8*3;
            int tx = (WIN_W - tw)/2, ty = WIN_H - STATUS_H - WUOS_SPACE_8*5;
            SDL_SetRenderDrawColor(ren, dark?WUOS_DARK(OVERLAY_SURFACE).r:WUOS_LIGHT(OVERLAY_SURFACE).r, dark?WUOS_DARK(OVERLAY_SURFACE).g:WUOS_LIGHT(OVERLAY_SURFACE).g, dark?WUOS_DARK(OVERLAY_SURFACE).b:WUOS_LIGHT(OVERLAY_SURFACE).b,(Uint8)(235*ta));
            SDL_RenderFillRect(ren,&(SDL_Rect){tx,ty,tw,WUOS_SPACE_8*4});
            SDL_SetRenderDrawColor(ren, dark?WUOS_DARK(ACCENT).r:WUOS_LIGHT(ACCENT).r, dark?WUOS_DARK(ACCENT).g:WUOS_LIGHT(ACCENT).g, dark?WUOS_DARK(ACCENT).b:WUOS_LIGHT(ACCENT).b, 255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){tx,ty,tw,WUOS_SPACE_8*4});
            sdl_text(ren, tx+WUOS_SPACE_8*5, ty+WUOS_SPACE_8, dark?WUOS_DARK(OVERLAY_TEXT).r:WUOS_LIGHT(OVERLAY_TEXT).r, dark?WUOS_DARK(OVERLAY_TEXT).g:WUOS_LIGHT(OVERLAY_TEXT).g, dark?WUOS_DARK(OVERLAY_TEXT).b:WUOS_LIGHT(OVERLAY_TEXT).b, tt);
        }

        /* UI-29: command palette overlay (top-center) */
        if (palette_is_open(g_palette)){
            int pw = WUOS_SPACE_32*13 + WUOS_SPACE_4, px = (WIN_W-pw)/2, py = TAB_H + WUOS_SPACE_4*6;
            int rows = palette_result_count(g_palette); if (rows>8) rows=8;
            int ph = WUOS_SPACE_8*5 + rows*WUOS_SPACE_26 + WUOS_SPACE_8;
            WuosRGB pal_bg  = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
            WuosRGB pal_bd  = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
            WuosRGB pal_hl  = dark ? WUOS_DARK(OVERLAY_HIGHLIGHT) : WUOS_LIGHT(OVERLAY_HIGHLIGHT);
            WuosRGB pal_txt = dark ? WUOS_DARK(OVERLAY_TEXT)    : WUOS_LIGHT(OVERLAY_TEXT);
            WuosRGB pal_hnt = dark ? WUOS_DARK(OVERLAY_HINTS)    : WUOS_LIGHT(OVERLAY_HINTS);
            SDL_SetRenderDrawColor(ren, pal_bg.r, pal_bg.g, pal_bg.b, 245);
            SDL_RenderFillRect(ren,&(SDL_Rect){px,py,pw,ph});
            SDL_SetRenderDrawColor(ren, pal_bd.r, pal_bd.g, pal_bd.b, 255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){px,py,pw,ph});
            char qline[96];
            snprintf(qline,sizeof qline,"> %s", palette_query(g_palette));
            sdl_text(ren, px+WUOS_SPACE_8*2, py+WUOS_SPACE_8, pal_hnt.r,pal_hnt.g,pal_hnt.b, qline);
            for (int i=0;i<rows;i++){
                if (i==palette_selected(g_palette)){
                    SDL_SetRenderDrawColor(ren,pal_hl.r,pal_hl.g,pal_hl.b,255);
                    SDL_RenderFillRect(ren,&(SDL_Rect){px+WUOS_SPACE_4,py+WUOS_SPACE_8*5+i*WUOS_SPACE_26,pw-WUOS_SPACE_8,WUOS_SPACE_24});
                }
                sdl_text(ren, px+WUOS_SPACE_8*2, py+WUOS_SPACE_8*5+i*WUOS_SPACE_26, pal_txt.r,pal_txt.g,pal_txt.b,
                         palette_result_label(g_palette, i));
            }
        }

        /* UI-36: shortcut cheat-sheet overlay (F1) */
        if (g_cheat){
            int cw = WUOS_SPACE_32*14 + WUOS_SPACE_4, cx = (WIN_W-cw)/2, cy = TAB_H + WUOS_SPACE_8*5;
            int ch = WUOS_SPACE_32*10;
            WuosRGB cheat_bg = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
            WuosRGB cheat_bd = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
            WuosRGB cheat_ttl= dark ? WUOS_DARK(OVERLINE_TEXT)  : WUOS_LIGHT(OVERLINE_TEXT);
            WuosRGB cheat_txt= dark ? WUOS_DARK(OVERLAY_TEXT)    : WUOS_LIGHT(OVERLAY_TEXT);
            SDL_SetRenderDrawColor(ren, cheat_bg.r, cheat_bg.g, cheat_bg.b, 250);
            SDL_RenderFillRect(ren,&(SDL_Rect){cx,cy,cw,ch});
            SDL_SetRenderDrawColor(ren, cheat_bd.r, cheat_bd.g, cheat_bd.b, 255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){cx,cy,cw,ch});
            sdl_text(ren, cx+WUOS_SPACE_8*2, cy+WUOS_SPACE_8, cheat_ttl.r,cheat_ttl.g,cheat_ttl.b, "Keyboard shortcuts");
            const char *keys[] = {
                "Ctrl+K   command palette",
                "Ctrl+`   toggle dark/light theme",
                "Ctrl+C    high-contrast (in Settings)",
                "F1        this cheat sheet",
                "Ctrl+1..6 jump to TOC heading (Document)",
                "Ctrl+F    find   Ctrl+G go-to line",
                "Ctrl+=/-  zoom in / out   Ctrl+0 reset",
                "Ctrl+S    save   Ctrl+W close tab",
                "Ctrl+Tab  next tab  Ctrl+Shift+Tab prev",
                "F10        open Settings",
                "Ctrl+L    insert hyperlink (Document)",
                "Ctrl+Shift+L insert bullet list (Document)",
                "Ctrl+Shift+T insert table (Document)",
                "Ctrl+Shift+I insert image (Document)",
                "Ctrl+Shift+B page break (Document)",
                "Ctrl+Shift+S section break (Document)",
                "Ctrl+Shift+H header (Document)",
                "Ctrl+Shift+F footer (Document)",
                "Ctrl+Shift+C comment (Document)",
                "Ctrl+Shift+Alt+T track-change (Document)",
                "Ctrl+Shift+D field (Document)",
                "Ctrl+Alt+1/2/3 Heading 1/2/3 style (Document)",
                "Ctrl+Shift+Up/Down move style target para",
                "Ctrl+K then 'Style:' pick a preset",
                "Right-click context menu",
                "Drag & drop a file to open"
            };
            for (int i=0;i<26;i++)
                sdl_text(ren, cx+WUOS_SPACE_8*2, cy+WUOS_SPACE_8*5+i*WUOS_SPACE_26, cheat_txt.r,cheat_txt.g,cheat_txt.b, keys[i]);
        }

        /* UI-30: first-run onboarding splash (dismiss with any key) */
        if (g_first_run){
            int pw = WUOS_SPACE_32*16 + WUOS_SPACE_8*2, px = (WIN_W-pw)/2, py = TAB_H + WUOS_SPACE_4*8;
            int ph = WUOS_SPACE_32*11;
            WuosRGB sp_bg  = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
            WuosRGB sp_bd  = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
            WuosRGB sp_ttl = dark ? WUOS_DARK(OVERLINE_TEXT)  : WUOS_LIGHT(OVERLINE_TEXT);
            WuosRGB sp_txt = dark ? WUOS_DARK(OVERLAY_TEXT)    : WUOS_LIGHT(OVERLAY_TEXT);
            SDL_SetRenderDrawColor(ren, sp_bg.r, sp_bg.g, sp_bg.b, 252);
            SDL_RenderFillRect(ren,&(SDL_Rect){px,py,pw,ph});
            SDL_SetRenderDrawColor(ren, sp_bd.r, sp_bd.g, sp_bd.b, 255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){px,py,pw,ph});
            sdl_text(ren, px+WUOS_SPACE_8*2, py+WUOS_SPACE_8*2, sp_ttl.r,sp_ttl.g,sp_ttl.b, "Welcome to WuBuOffice");
            const char *tips[] = {
                "A clean-room office suite (word processor, editor,",
                "spreadsheet, OCR, slides) - no third-party frameworks.",
                "",
                "Ctrl+K     command palette (every action lives here)",
                "Ctrl+F     find      Ctrl+G   go to line",
                "Ctrl+`     toggle dark / light theme",
                "Ctrl+Shift+R record a macro, Ctrl+Shift+P play it back",
                "F1          full keyboard cheat-sheet",
                "Drag & drop a file to open it",
                "",
                "Press any key to start"
            };
            for (int i=0;i<11;i++)
                sdl_text(ren, px+WUOS_SPACE_8*2, py+WUOS_SPACE_8*6+i*WUOS_SPACE_26, sp_txt.r,sp_txt.g,sp_txt.b, tips[i]);
        }
        /* WUOS_DUMP: PPM on first frame. WUOS_DUMP_VIEW switches the active
         * view first; WUOS_DUMP_MENU=<0..3> opens that dropdown so headless
         * captures show accelerators. */
        {
            static const char *dump = NULL;
            static int dumped = 0, frames = 0;
            if (!dump){
                dump = getenv("WUOS_DUMP");
                const char *dm = getenv("WUOS_DUMP_MENU");
                if (dm && *dm){ int mi = atoi(dm); if (mi>=0 && (size_t)mi<g_nmenus){ g_menu_open=mi; g_menu_hover=mi; } }
                const char *dz = getenv("WUOS_DUMP_ZOOM");
                if (dz && *dz){ g_zoom = (float)atof(dz); apply_zoom(); }
            }
            frames++;
            if (dump && !dumped && frames >= 12){   /* wait for view+font render */
                void *pix = malloc((size_t)WIN_W*WIN_H*4);
                if (pix && SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ABGR8888,
                                                pix, WIN_W*4) == 0){
                    FILE *f = fopen(dump, "wb");
                    if (f){
                        fprintf(f, "P6\n%d %d\n255\n", WIN_W, WIN_H);
                        unsigned char *p = pix;
                        for (int i=0;i<WIN_W*WIN_H;i++){
                            unsigned char *px = p + (size_t)i*4;
                            fputc(px[2], f); /* R */ fputc(px[1], f); /* G */ fputc(px[0], f); /* B */
                        }
                        fclose(f);
                    }
                    dumped = 1;
                }
                free(pix);
            }
        }
        SDL_Delay(16);

        /* advance micro-interaction tweens (dt from real time; 16ms target).
         * prefers-reduced-motion is honored by leaving the tweens at their
         * targets in the render (see reduce checks). */
        {   Uint32 now = SDL_GetTicks();
            static Uint32 last = 0;
            float dt = (last ? (float)(now - last) : 16.0f) / 1000.0f;
            if (dt > 0.1f) dt = 0.1f;   /* clamp on stalls */
            last = now;
            wuos_tween_advance(&g_tab_ul, dt);
            g_caret_phase = now;
        }
    }

    for (int i=0;i<nviews;i++) views[i]->destroy(views[i]);
    free(g_plugin_msg);
    toast_destroy(g_toasts);
    palette_destroy(g_palette);
    hive_free(g_hive);   /* data-driven menu/toolbar/slide template */
    wuos_tb_shutdown();  /* free the hive-built toolbar table */
    wuos_plugins_free(g_plugins);
    wuos_font_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
