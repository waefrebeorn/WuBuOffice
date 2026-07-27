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

int main(int argc, char **argv){
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO)!=0){ fprintf(stderr,"SDL init: %s\n",SDL_GetError()); return 1; }
    if (wuos_font_init()!=0){ fprintf(stderr,"font init failed\n"); SDL_Quit(); return 1; }

    SDL_Window *win = SDL_CreateWindow("WuBuOffice", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    if (!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    add_view(wuos_doc_create());
    add_view(wuos_cell_create());
    add_view(wuos_slide_create());
    add_view(wuos_ocr_create());
    add_view(wuos_editor_create());
    if (nviews==0){ fprintf(stderr,"no views\n"); return 1; }

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
                int code=0;
                if (k==SDLK_ESCAPE){ running=0; }
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
            SDL_SetRenderDrawColor(ren, on?59:200, on?130:205, on?246:210, 255);
            SDL_RenderFillRect(ren,&(SDL_Rect){x,0,tw,TAB_H});
            x+=tw;
        }
        /* status bar */
        char *st = views[active]->status? views[active]->status(views[active]) : NULL;
        SDL_SetRenderDrawColor(ren, 30,33,40,255); SDL_RenderFillRect(ren,&(SDL_Rect){0,WIN_H-STATUS_H,WIN_W,STATUS_H});
        if (st) free(st);

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
