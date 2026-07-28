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

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WIN_W 960
#define WIN_H 720
#define TAB_H 30
#define STATUS_H 26

static WuView *views[8];
static int     nviews = 0;
static int     active = 0;
static int     tab_hover = -1;
static float   g_zoom = 1.0f;       /* UI-24: shell-level zoom */
static int     g_ctx = 0;           /* UI-27: context menu open? */
static int     g_ctx_item = 0;      /* highlighted item */
static int     g_ctx_x = 0, g_ctx_y = 0;
static Toasts  *g_toasts = NULL;    /* UI-33: toast queue */
static Palette *g_palette = NULL;   /* UI-29: command palette (Ctrl+K) */
static int      g_cheat = 0;       /* UI-36: shortcut cheat-sheet overlay */
static int      g_first_run = 0;    /* UI-30: first-run onboarding splash */
static Dialog  *g_dlg = NULL;       /* modal text-input dialog */
static int      g_dlg_action = 0;   /* 0 none,1 link,2 qr,3 image-alt */
/* Plugin manager: loaded once at startup from ~/.wubuos/plugins. */
static WuOSPluginMgr *g_plugins = NULL;
static char   *g_plugin_msg = NULL;   /* last exec() result toast */
static int     g_plugin_idx = 0;      /* next plugin to run via Ctrl+Shift+K */

static void add_view(WuView *v){ if (v && nviews<8) views[nviews++]=v; }

/* Open `path` in a suitable tab (editor for text-ish, document otherwise),
 * make it active, and record it in the recent-documents list (UI-39). */
static void open_doc_path(const char *path){
    if (!path || !*path) return;
    size_t L = strlen(path);
    int is_text = (L>3 && (!strcmp(path+L-3,".md")||!strcmp(path+L-3,".c")||
                  !strcmp(path+L-2,".h")||!strcmp(path+L-3,".py")||
                  !strcmp(path+L-4,".txt")));
    WuView *nv = is_text ? wuos_editor_create(path) : wuos_doc_create(path);
    if (!nv) return;
    if (nviews < 8){ add_view(nv); active = nviews-1; }
    WubuSettings *sh = wubusettings_shared();
    if (sh){ wubusettings_add_recent(sh, path); wubusettings_save(sh, NULL); }
}

static int tab_at(int mx){
    int x=0;
    for (int i=0;i<nviews;i++){ int tw = (int)strlen(views[i]->name)*14 + 24; if (mx>=x && mx<x+tw) return i; x+=tw; }
    return -1;
}

/* Paint UTF-8 `text` at (px,py) directly onto the SDL renderer using the
 * shared FreeType helper (draws into a 1-line RGBA strip, uploads as texture). */
static void sdl_text(SDL_Renderer *ren, int px, int py,
                     unsigned char r, unsigned char g, unsigned char b,
                     const char *text){
    if (!text || !*text) return;
    int fh = wuos_font_height();
    int wpx = wuos_font_draw(text, 0, 0, 0, 0,0,0, NULL, 0, 0);
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

    SDL_Window *win = SDL_CreateWindow("WuBuOffice", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    if (!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    /* load plugins from ~/.wubuos/plugins (if present) */
    g_plugins = wuos_plugins_load(NULL);
    if (g_plugins && wuos_plugins_count(g_plugins)==0)
        fprintf(stderr, "[plugin] no modules in ~/.wubuos/plugins (ok)\n");

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
    palette_add(g_palette, "Insert: Hyperlink", 40);
    palette_add(g_palette, "Insert: QR Code",   41);
    palette_add(g_palette, "Insert: Image (alt text)", 42);
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
            }
            else if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                if (e.button.y < TAB_H){ int t=tab_at(e.button.x); if(t>=0){ active=t; scroll=0; } }
                else if (g_ctx){  /* UI-27: select context-menu item */
                    /* items: 0 Open File, 1 New Document, 2 Toggle Theme */
                    if (g_ctx_item==2){
                        WubuSettings *sh=wubusettings_shared(); if(sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                    }
                    g_ctx = 0;
                }
                else if (views[active]->on_click){  /* clickable links/objects */
                    int lx = e.button.x;
                    int ly = e.button.y - TAB_H;
                    if (ly >= 0) views[active]->on_click(views[active], lx, ly);
                }
            }
            else if (e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_RIGHT){
                g_ctx = 1; g_ctx_item = 0; g_ctx_x = e.button.x; g_ctx_y = e.button.y;  /* UI-27 */
            }
            else if (e.type==SDL_MOUSEMOTION && g_ctx){
                /* highlight the item under the cursor (3 items, 26px tall) */
                int rel = e.motion.y - g_ctx_y - 4;
                g_ctx_item = (rel>=0)? rel/26 : 0; if (g_ctx_item>2) g_ctx_item=2;
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
                        case 4: g_zoom += 0.1f; if (g_zoom>3.0f) g_zoom=3.0f;
                                toast_push(g_toasts, "Zoom in", 60); break;
                        case 5: g_zoom -= 0.1f; if (g_zoom<0.5f) g_zoom=0.5f;
                                toast_push(g_toasts, "Zoom out", 60); break;
                        case 6: g_zoom = 1.0f; toast_push(g_toasts, "Zoom reset", 60); break;
                        case 7: for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; scroll=0; break; }
                                break;
                        case 8: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_EPUB, 1);
                                toast_push(g_toasts, "EPUB export requested", 120); break;
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
                                   else toast_push(g_toasts, "Macro save failed", 120); } break;
                        case 33: { const char *mp = getenv("WUBUOS_MACRO_DIR");
                                   char buf[512];
                                   snprintf(buf,sizeof buf,"%s/wubuos_macro.mac", mp? mp : "/tmp");
                                   if (macro_load(buf)==0) toast_push(g_toasts, "Macro loaded", 90);
                                   else toast_push(g_toasts, "Macro load failed", 120); } break;
                        /* DOC-66 / EXP-89 / UXA-47: open the modal dialog. */
                        case 40: g_dlg_action = 1; dialog_open(g_dlg, "Insert Hyperlink", "URL:", "https://"); toast_push(g_toasts, "Hyperlink: type URL, Enter", 120); break;
                        case 41: g_dlg_action = 2; dialog_open(g_dlg, "Insert QR Code", "Text:", ""); toast_push(g_toasts, "QR: type text, Enter", 120); break;
                        case 42: g_dlg_action = 3; dialog_open(g_dlg, "Insert Image", "Alt text:", ""); toast_push(g_toasts, "Image: type alt text, Enter", 120); break;
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
                                    } else toast_push(g_toasts, "Font switch failed", 120);
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

                /* ---- shell-level features (not forwarded to the view) ---- */
                if (code == WUOS_KEY_ZOOM_IN){
                    g_zoom += 0.1f; if (g_zoom>3.0f) g_zoom=3.0f;
                    WubuSettings *sh = wubusettings_shared(); if (sh) wubusettings_set_zoom(sh, g_zoom);
                } else if (code == WUOS_KEY_ZOOM_OUT){
                    g_zoom -= 0.1f; if (g_zoom<0.5f) g_zoom=0.5f;
                    WubuSettings *sh = wubusettings_shared(); if (sh) wubusettings_set_zoom(sh, g_zoom);
                } else if (code == WUOS_KEY_ZOOM_RESET){
                    g_zoom = 1.0f;
                    WubuSettings *sh = wubusettings_shared(); if (sh) wubusettings_set_zoom(sh, g_zoom);
                } else if (code == WUOS_KEY_SETTINGS){
                    for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; scroll=0; break; }
                } else if (code == WUOS_KEY_CHEAT){
                    g_cheat = !g_cheat;   /* UI-36 toggle */
                }
            }
        }

        /* render active view */
        unsigned char *rgba=NULL; int rw=0, rh=0;
        if (views[active]->render(views[active], WIN_W, WIN_H - TAB_H - STATUS_H, scroll, &rgba, &rw, &rh)!=0)
            rgba=NULL;

        SDL_SetRenderDrawColor(ren, 235,237,240,255);
        SDL_RenderClear(ren);
        if (rgba){
            SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, rw, rh);
            if (tex){
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
                SDL_UpdateTexture(tex, NULL, rgba, rw*4);
                int maxscroll = (rh > (WIN_H-TAB_H-STATUS_H))? rh-(WIN_H-TAB_H-STATUS_H):0;
                if (scroll>maxscroll) scroll=maxscroll;
                /* UI-24: apply shell zoom by scaling the destination rect */
                int draw_w = (int)(rw * g_zoom);
                int draw_h = (int)(rh * g_zoom);
                SDL_Rect src={0,scroll,rw,rh}; SDL_Rect dst={0,TAB_H,draw_w,draw_h};
                if (draw_h < (WIN_H-TAB_H-STATUS_H)){ dst.y=TAB_H; dst.h=draw_h; src.h=rh; }
                else { src.h=(WIN_H-TAB_H-STATUS_H); dst.h=(WIN_H-TAB_H-STATUS_H); }
                SDL_RenderCopy(ren, tex, &src, &dst);
                SDL_DestroyTexture(tex);
            }
            free(rgba);
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
            int tw = (int)strlen(views[i]->name)*14 + 24;
            int on = (i==active);
            int hov = (i==tab_hover);
            WuosRGB seg = on ? tto : ttk;
            SDL_SetRenderDrawColor(ren, seg.r, seg.g, seg.b, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x,0,tw,TAB_H});
            /* 1px right divider */
            SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x+tw-1,0,1,TAB_H});
            /* active tab: 2px accent underline */
            if (on){
                SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, 255);
                SDL_RenderFillRect(ren,&(SDL_Rect){x,TAB_H-2,tw,2});
            }
            sdl_text(ren, x+12, (TAB_H-wuos_font_height())/2 + 2,
                     on?ttxo.r:ttx.r, on?ttxo.g:ttx.g, on?ttxo.b:ttx.b,
                     views[i]->name);
            x+=tw;
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
            sdl_text(ren, 12, WIN_H-STATUS_H + (STATUS_H-wuos_font_height())/2 + 1,
                     stx.r, stx.g, stx.b, st);
            free(st);
        }

        /* plugin toast (Ctrl+Shift+K result) */
        if (g_plugin_msg){
            sdl_text(ren, WIN_W-360, WIN_H-STATUS_H + (STATUS_H-wuos_font_height())/2 + 1,
                     120,220,140, g_plugin_msg);
        }

        /* UI-27: right-click context menu overlay */
        if (g_ctx){
            const char *items[3] = { "Open File…", "New Document", "Toggle Theme" };
            int mw = 160, mh = 3*26 + 8;
            int mx = g_ctx_x, my = g_ctx_y;
            if (mx+mw > WIN_W) mx = WIN_W-mw; if (my+mh > WIN_H-STATUS_H) my = WIN_H-STATUS_H-mh;
            SDL_SetRenderDrawColor(ren, 26,29,36,255); SDL_RenderFillRect(ren,&(SDL_Rect){mx,my,mw,mh});
            SDL_SetRenderDrawColor(ren, 70,76,90,255); SDL_RenderDrawRect(ren,&(SDL_Rect){mx,my,mw,mh});
            for (int i=0;i<3;i++){
                if (i==g_ctx_item){ SDL_SetRenderDrawColor(ren,94,135,255,255); SDL_RenderFillRect(ren,&(SDL_Rect){mx+2,my+4+i*26,mw-4,24}); }
                sdl_text(ren, mx+10, my+8+i*26, 220,223,230, items[i]);
            }
        }

        /* modal text-input dialog overlay (DOC-66/EXP-89/UXA-47) */
        if (dialog_active(g_dlg)){
            int bw = 520, bx = (WIN_W-bw)/2, by = TAB_H + 80, bh = 120;
            SDL_SetRenderDrawColor(ren, 26,29,36,252); SDL_RenderFillRect(ren,&(SDL_Rect){bx,by,bw,bh});
            SDL_SetRenderDrawColor(ren, 70,76,90,255); SDL_RenderDrawRect(ren,&(SDL_Rect){bx,by,bw,bh});
            sdl_text(ren, bx+20, by+12, 240,243,250, dialog_title(g_dlg));
            sdl_text(ren, bx+20, by+40, 200,203,210, dialog_prompt(g_dlg));
            sdl_text(ren, bx+20, by+68, 230,233,240, dialog_text(g_dlg));
            /* caret */
            int cw = wuos_font_text_width(dialog_text(g_dlg), 20);
            SDL_SetRenderDrawColor(ren, 230,233,240,255);
            SDL_RenderFillRect(ren,&(SDL_Rect){bx+20+cw, by+66, 2, 20});
            sdl_text(ren, bx+20, by+bh-18, 150,153,160, "Enter to confirm · Esc to cancel");
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
            int tw = (int)strlen(tt)*8 + 24;
            int tx = (WIN_W - tw)/2, ty = WIN_H - STATUS_H - 42;
            SDL_SetRenderDrawColor(ren, 28,31,38,(Uint8)(235*ta));
            SDL_RenderFillRect(ren,&(SDL_Rect){tx,ty,tw,30});
            SDL_SetRenderDrawColor(ren, 94,135,255,255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){tx,ty,tw,30});
            sdl_text(ren, tx+12, ty+8, 220,223,230, tt);
        }

        /* UI-29: command palette overlay (top-center) */
        if (palette_is_open(g_palette)){
            int pw = 420, px = (WIN_W-pw)/2, py = TAB_H + 24;
            int rows = palette_result_count(g_palette); if (rows>8) rows=8;
            int ph = 40 + rows*26 + 8;
            SDL_SetRenderDrawColor(ren, 26,29,36,245);
            SDL_RenderFillRect(ren,&(SDL_Rect){px,py,pw,ph});
            SDL_SetRenderDrawColor(ren, 70,76,90,255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){px,py,pw,ph});
            char qline[96];
            snprintf(qline,sizeof qline,"> %s", palette_query(g_palette));
            sdl_text(ren, px+12, py+10, 240,243,250, qline);
            for (int i=0;i<rows;i++){
                if (i==palette_selected(g_palette)){
                    SDL_SetRenderDrawColor(ren,94,135,255,255);
                    SDL_RenderFillRect(ren,&(SDL_Rect){px+4,py+40+i*26,pw-8,24});
                }
                sdl_text(ren, px+14, py+44+i*26, 220,223,230,
                         palette_result_label(g_palette, i));
            }
        }

        /* UI-36: shortcut cheat-sheet overlay (F1) */
        if (g_cheat){
            int cw = 460, cx = (WIN_W-cw)/2, cy = TAB_H + 40;
            int ch = 320;
            SDL_SetRenderDrawColor(ren, 20,22,28,250);
            SDL_RenderFillRect(ren,&(SDL_Rect){cx,cy,cw,ch});
            SDL_SetRenderDrawColor(ren, 70,76,90,255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){cx,cy,cw,ch});
            sdl_text(ren, cx+14, cy+10, 240,243,250, "Keyboard shortcuts");
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
                sdl_text(ren, cx+14, cy+40+i*22, 200,203,210, keys[i]);
        }

        /* UI-30: first-run onboarding splash (dismiss with any key) */
        if (g_first_run){
            int pw = 520, px = (WIN_W-pw)/2, py = TAB_H + 30;
            int ph = 360;
            SDL_SetRenderDrawColor(ren, 18,20,26,252);
            SDL_RenderFillRect(ren,&(SDL_Rect){px,py,pw,ph});
            SDL_SetRenderDrawColor(ren, 70,76,90,255);
            SDL_RenderDrawRect(ren,&(SDL_Rect){px,py,pw,ph});
            sdl_text(ren, px+20, py+16, 240,243,250, "Welcome to WuBuOffice");
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
                sdl_text(ren, px+20, py+52+i*26, 200,203,210, tips[i]);
        }
        SDL_Delay(16);
    }

    for (int i=0;i<nviews;i++) views[i]->destroy(views[i]);
    free(g_plugin_msg);
    toast_destroy(g_toasts);
    palette_destroy(g_palette);
    wuos_plugins_free(g_plugins);
    wuos_font_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
