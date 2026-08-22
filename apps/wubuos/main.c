/* wubuos -- unified WuBuOffice GUI shell (the "whole suite + Notepad++" front
 * end). One SDL2 window hosts every engine behind a WuView adapter selected by
 * a top tab bar. The shell owns the window, tab bar, status bar and g_scroll, and
 * dispatches events to the active view. Renders are done by the shared
 * wuburender / WuBuPad core / wubucell / wubuocr engines -- no mockups. */
#include "wuos.h"
#include "wuos_shell_internal.h"
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

/* Chrome heights derive from the base font size (Haiku-DPI pattern, research
 * 2026-08-12): zooming scales the whole UI, not just the view rect. Each is
 * rounded to the 8pt grid (multiple of 4) so all chrome aligns to the spacing
 * scale (Atlassian/USWDS research). At default fh=20 they are 32/24/28/28/224.
 * Note: the tricorder's region coordinates in gui_audit_test.sh must match
 * these defaults (tabs 0-32, menu 32-56, toolbar 56-84, status 692-720). */

WuView *views[8];
int     nviews = 0;
int     active = 0;
int     tab_hover = -1;
int     tab_drag_from = -1;  /* index of tab being dragged, -1 none */
int     tab_drag_x = 0;      /* pointer x at drag start */
float   g_zoom = 1.0f;       /* UI-24: shell-level zoom */
int     g_zoom_drag = 0;     /* zoom slider being dragged */
int     g_sidebar = 1;       /* docked Navigator panel shown? */
int     g_ctx = 0;           /* UI-27: context menu open? */
int     g_ctx_item = 0;      /* highlighted item */
int     g_ctx_x = 0, g_ctx_y = 0;
int     g_scroll = 0;      /* shared shell scroll (px) */
Toasts  *g_toasts = NULL;    /* UI-33: toast queue */
Palette *g_palette = NULL;   /* UI-29: command palette (Ctrl+K) */

/* ---- micro-interaction / animation state (GUI_EXCELLENCE emotional design,
 * GUI_MATHEMATICS timing & motion). All honored by prefers-reduced-motion. --- */
WuosTween g_tab_ul;      /* sliding active-tab underline (x0 -> x1) */
int       g_tab_ul_from = 0, g_tab_ul_to = 0; /* underline span (px) */
float     g_tb_press_t = 0; /* toolbar button press timestamp (ms) */
int       g_tb_press_i = -1;/* toolbar button index being pressed */
Uint32    g_caret_phase = 0;/* caret blink phase (ms) */
int      g_cheat = 0;       /* UI-36: shortcut cheat-sheet overlay */
int      g_first_run = 0;    /* UI-30: first-run onboarding splash */
Dialog  *g_dlg = NULL;       /* modal text-input dialog */
int      g_dlg_action = 0;   /* 0 none,1 link,2 qr,3 image-alt,10 open,11 save-as */
/* Menu bar + toolbar (UI-43): data-driven from the hive template (hive.json),
 * NOT hardcoded arrays. Selecting an item runs a command id (same space the
 * command palette uses). Pure chrome state here; views stay GUI-free. */
Hive *g_hive = NULL;         /* loaded at startup (no-hardcoding) */
const HiveMenu *g_menus = NULL;  /* shorthand for hive_menus() */
size_t g_nmenus = 0;
int g_menu_open = -1;       /* which top menu is dropped, -1 none */
int g_menu_hover = -1;      /* hovered dropdown item */

int g_tb_hover = -1;        /* hovered toolbar button index (model in
                                    * wuos_toolbar.{h,c}) */

/* Set zoom from a status-bar x position over the zoom slider track. Shared by
 * the click and drag paths so they behave identically. */
void zoom_from_x(int x){
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
void apply_zoom(void){
    if (g_zoom < 0.5f) g_zoom = 0.5f;
    if (g_zoom > 3.0f) g_zoom = 3.0f;
    wuos_font_set_size((int)(20.0 * g_zoom));
    WubuSettings *sh=wubusettings_shared(); if(sh) wubusettings_set_zoom(sh, g_zoom);
}

void run_menu_cmd(int cmd);   /* defined below; used by toolbar buttons */

/* Run a toolbar button: menu-space commands via run_menu_cmd, formatting/
 * insert buttons forwarded to the active view's on_key. */
void run_tb_cmd(int cmd){
    int k = wuos_tb_cmd_to_key(cmd);
    if (k){ if (views[active]->on_key) views[active]->on_key(views[active], k, 1); return; }
    run_menu_cmd(cmd);
}

/* Plugin manager: loaded once at startup from ~/.wubuos/plugins. */
WuOSPluginMgr *g_plugins = NULL;
char   *g_plugin_msg = NULL;   /* last exec() result toast */
int     g_plugin_idx = 0;      /* next plugin to run via Ctrl+Shift+K */

void add_view(WuView *v){ if (v && nviews<8) views[nviews++]=v; }

/* Run a menu command id (UI-43). Shares the action space with the command
 * palette where possible. `cmd` 1000-1034 are menu-specific. */
void run_menu_cmd(int cmd){
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

int tab_width(int i);   /* fwd (defined below; uses real font width) */

int tab_at(int mx){
    int x=0;
    for (int i=0;i<nviews;i++){ int tw = tab_width(i); if (mx>=x && mx<x+tw) return i; x+=tw; }
    return -1;
}

/* Tab width (must stay in sync with the render loop + tab_at). Uses the REAL
 * font advance (11px/char @20, not a hardcoded 14) so tabs don't overlap. */
int tab_width(int i){ return wuos_font_text_width(views[i]->name, wuos_font_height()) + 24; }

/* Real text width of a label at the current font size (fixed the hardcoded
 * 7px/char estimate that under-sized buttons and made them overlap). */
int wu_text_w(const char *s){ return wuos_font_text_width(s, wuos_font_height()); }

/* Reorder the tab at `from` to position `to` (indices into views). Updates
 * active to follow the dragged tab. Used by drag-to-reorder. */
void tab_reorder(int from, int to){
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
int center_text_y(int band_h){
    return (band_h - (wuos_font_height() + 6)) / 2;
}

void sdl_text(SDL_Renderer *ren, int px, int py,
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

    g_scroll = 0;
    int running = 1;
    SDL_Event e;

    while (running){
        while (SDL_PollEvent(&e)){
            if (e.type==SDL_QUIT){ running=0; }
            else if (e.type==SDL_MOUSEWHEEL){
                g_scroll += e.wheel.y*40;
                if (g_scroll<0) g_scroll=0;
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
                    active=t; g_scroll=0; tab_drag_from=t; tab_drag_x=e.button.x; } }
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
                                  if (nv && nviews<8){ add_view(nv); active=nviews-1; g_scroll=0; }
                                  toast_push(g_toasts, "New document", 90); } break;
                        case 3: { WubuSettings *sh=wubusettings_shared();
                                  if (sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                                  toast_push(g_toasts, "Theme toggled", 90); } break;
                        case 4: g_zoom += 0.1f; apply_zoom();
                                toast_push(g_toasts, "Zoom in", 60); break;
                        case 5: g_zoom -= 0.1f; apply_zoom();
                                toast_push(g_toasts, "Zoom out", 60); break;
                        case 6: g_zoom = 1.0f; apply_zoom(); toast_push(g_toasts, "Zoom reset", 60); break;
                        case 7: for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; g_scroll=0; break; }
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
                    for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; g_scroll=0; break; }
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
                    if (views[i]->name && !strcmp(views[i]->name, dv)){ active=i; g_scroll=0; break; }
                view_switched = 1;
            }
        }
        wuos_frame_render(ren);
    
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
