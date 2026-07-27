/* wubuwordwin -- interactive WuBuWord window (the office suite, LIVE).
 *
 * This is the real, usable face of the suite: it opens an SDL2 window and
 * presents the SAME wuburender output the offscreen PNG writer uses, so the
 * engine and the UI can never diverge. It is interactive:
 *   - mouse wheel / Up-Down arrows scroll the page vertically
 *   - typing in the bottom input line appends a paragraph to the document
 *     and re-renders live (proves model -> render -> screen round-trips)
 *   - Esc or closing the window quits
 *
 * Uses system SDL2 (allowed system lib, not bundled third-party).
 *
 * NOTE: this is a SEPARATE app from apps/wubuword (the WordprocessingML
 * editor). wuburender is the shared render path both could use.
 */
#include "wuburender.h"
#include "model.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 900
#define WIN_H 760
#define PAGE_H 1200          /* rendered page height (tall; we scroll it) */

static SDL_Texture *make_texture(SDL_Renderer *ren, const unsigned char *rgba,
                                  int w, int h){
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) return NULL;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_UpdateTexture(tex, NULL, rgba, w*4);
    return tex;
}

/* Re-render the doc and swap the texture. Returns 0 ok. */
static int rerender(Wurender *r, wubumodel_doc *doc, SDL_Renderer *ren,
                    SDL_Texture **tex){
    unsigned char *rgba; int w, h;
    if (wurender_render_doc(r, doc, WIN_W, PAGE_H, &rgba, &w, &h) != 0) return -1;
    SDL_Texture *nt = make_texture(ren, rgba, w, h);
    free(rgba);
    if (!nt) return -1;
    if (*tex) SDL_DestroyTexture(*tex);
    *tex = nt;
    return 0;
}

int main(int argc, char **argv){
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) != 0){
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("WuBuWord",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
    if (!win){ fprintf(stderr, "window failed: %s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren){ fprintf(stderr, "renderer failed: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    Wurender *r = wurender_create();
    if (!r){ fprintf(stderr, "wurender init failed (no font?)\n"); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    wubumodel_doc *doc = wurender_sample_doc();

    SDL_Texture *tex = NULL;
    if (rerender(r, doc, ren, &tex) != 0){
        fprintf(stderr, "render failed\n");
        wubumodel_doc_destroy(doc); wurender_destroy(r);
        SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit(); return 1;
    }

    char input[512]; size_t ilen = 0; input[0]=0;
    int scroll = 0;
    int running = 1;
    SDL_Event e;

    while (running){
        while (SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT){ running = 0; }
            else if (e.type == SDL_KEYDOWN){
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE){ running = 0; }
                else if (k == SDLK_UP){ scroll -= 40; if (scroll<0) scroll=0; }
                else if (k == SDLK_DOWN){ scroll += 40; if (scroll>PAGE_H-WIN_H) scroll=PAGE_H-WIN_H; }
                else if (k == SDLK_BACKSPACE){
                    if (ilen>0){ ilen--; input[ilen]=0; }
                }
                else if (k == SDLK_RETURN){
                    if (ilen>0){
                        wubumodel_node *sec = wubumodel_doc_root(doc);
                        wubumodel_node *p = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
                        wubumodel_node *run = wubumodel_node_create(doc, WUBUMODEL_RUN);
                        wubumodel_run_set_text(run, input);
                        wubumodel_node_append(doc, p, run);
                        wubumodel_node_append(doc, sec, p);
                        rerender(r, doc, ren, &tex);
                    }
                    ilen = 0; input[0]=0;
                    scroll = 0;
                }
                else if (k >= 32 && k < 128 && ilen < sizeof input - 1){
                    input[ilen++] = (char)k; input[ilen]=0;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL){
                scroll += e.wheel.y * 40;
                if (scroll<0) scroll=0;
                if (scroll>PAGE_H-WIN_H) scroll=PAGE_H-WIN_H;
            }
        }

        SDL_SetRenderDrawColor(ren, 220, 222, 226, 255);
        SDL_RenderClear(ren);
        SDL_Rect src = { 0, scroll, WIN_W, WIN_H };
        SDL_Rect dst = { 0, 0, WIN_W, WIN_H };
        SDL_RenderCopy(ren, tex, &src, &dst);
        SDL_SetRenderDrawColor(ren, 30, 33, 40, 255);
        SDL_Rect bar = { 0, WIN_H-28, WIN_W, 28 };
        SDL_RenderFillRect(ren, &bar);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(tex);
    wubumodel_doc_destroy(doc);
    wurender_destroy(r);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
