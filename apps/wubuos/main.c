/* wubuos -- unified WuBuOffice GUI shell (the "whole suite + Notepad++" front
 * end). One SDL2 window hosts every engine behind a WuView adapter selected by
 * a top tab bar. The shell owns the window, tab bar, status bar and scroll, and
 * dispatches events to the active view. Renders are done by the shared
 * wuburender / WuBuPad core / wubucell / wubuocr engines -- no mockups. */
#include "wuos.h"
#include "wuos_font.h"

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

static void add_view(WuView *v){ if (v && nviews<8) views[nviews++]=v; }

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

    SDL_Window *win = SDL_CreateWindow("WuBuOffice", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    if (!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    add_view(wuos_doc_create(file_for_doc));
    add_view(wuos_cell_create(file_for_cell));
    add_view(wuos_slide_create(NULL));
    add_view(wuos_ocr_create(file_for_ocr));
    add_view(wuos_editor_create(file_for_editor));
    add_view(wuos_compare_create(argc>2?argv[2]:NULL, argc>3?argv[3]:NULL));
    if (nviews==0){ fprintf(stderr,"no views\n"); return 1; }
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
            }
            else if (e.type==SDL_KEYDOWN){
                SDL_Keycode k = e.key.keysym.sym;
                SDL_Keymod mod = SDL_GetModState();
                int code=0;
                if (k==SDLK_ESCAPE){ running=0; }
                else if (k==SDLK_s && (mod & KMOD_CTRL)) code=WUOS_KEY_SAVE;
                else if (k==SDLK_f && (mod & KMOD_CTRL)) code=WUOS_KEY_FIND;
                else if (k==SDLK_h && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACE;
                else if (k==SDLK_r && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACEALL;
                else if (k==SDLK_g && (mod & KMOD_CTRL)) code=WUOS_KEY_GOTO;
                else if (k==SDLK_e && (mod & KMOD_CTRL)) code=WUOS_KEY_EOL;
                else if (k==SDLK_BACKQUOTE && (mod & KMOD_CTRL)) code=WUOS_KEY_THEME;
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
                SDL_Rect src={0,scroll,rw,rh}; SDL_Rect dst={0,TAB_H,rw,rh};
                if (rh < (WIN_H-TAB_H-STATUS_H)){ dst.y=TAB_H; dst.h=rh; src.h=rh; }
                else { src.h=WIN_H-TAB_H-STATUS_H; dst.h=WIN_H-TAB_H-STATUS_H; }
                SDL_RenderCopy(ren, tex, &src, &dst);
                SDL_DestroyTexture(tex);
            }
            free(rgba);
        }

        /* tab bar */
        int x=0;
        SDL_SetRenderDrawColor(ren, 245,246,248,255); SDL_RenderFillRect(ren,&(SDL_Rect){0,0,WIN_W,TAB_H});
        for (int i=0;i<nviews;i++){
            int tw=(int)strlen(views[i]->name)*14+24;
            int on = (i==active);
            int hov = (i==tab_hover);
            SDL_SetRenderDrawColor(ren, on?59:(hov?225:200), on?130:(hov?228:205),
                                      on?246:(hov?232:210), 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x,0,tw,TAB_H});
            sdl_text(ren, x+12, (TAB_H-wuos_font_height())/2 + 2,
                     on?255:70, on?255:80, on?255:90, views[i]->name);
            x+=tw;
        }
        /* status bar */
        char *st = views[active]->status? views[active]->status(views[active]) : NULL;
        SDL_SetRenderDrawColor(ren, 30,33,40,255); SDL_RenderFillRect(ren,&(SDL_Rect){0,WIN_H-STATUS_H,WIN_W,STATUS_H});
        if (st){
            sdl_text(ren, 8, WIN_H-STATUS_H + (STATUS_H-wuos_font_height())/2 + 1,
                     200,203,210, st);
            free(st);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    for (int i=0;i<nviews;i++) views[i]->destroy(views[i]);
    wuos_font_quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
