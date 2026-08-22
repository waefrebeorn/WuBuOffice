/* wuos_frame.c -- one full frame of the wubuos shell: tab strip, menu bar,
 * toolbar, sidebar, active view, status bar, overlays, headless dump.
 * Split from main.c (no monoliths). State lives in main.c, declared in
 * wuos_shell_internal.h. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "wuos_shell_internal.h"
#include "wuos.h"
#include "wuos_font.h"
#include "hive.h"
#include "palette.h"
#include "toast.h"
#include "dialog.h"
#include "settings.h"
#include "wuos_toolbar.h"
#include "macro.h"
#include "wuos_plugin.h"
#include "layout.h"

void wuos_frame_render(SDL_Renderer *ren){
    (void)ren; /* used throughout via sdl_text + SDL calls below */
        /* render active view (placed below the tab strip, menu bar AND toolbar) */
        int view_top = TAB_H + MENU_H + TOOLBAR_H;
        int sb_w = (g_sidebar && views[active]->sidebar) ? SIDEBAR_W : 0;  /* dock sidebar */
        unsigned char *rgba=NULL; int rw=0, rh=0;
        if (views[active]->render(views[active], WIN_W - sb_w, WIN_H - view_top - STATUS_H, g_scroll, &rgba, &rw, &rh)!=0)
            rgba=NULL;

        SDL_SetRenderDrawColor(ren, 235,237,240,255);
        SDL_RenderClear(ren);
        if (rgba){
            SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, rw, rh);
            if (tex){
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
                SDL_UpdateTexture(tex, NULL, rgba, rw*4);
                int maxscroll = (rh > (WIN_H-view_top-STATUS_H))? rh-(WIN_H-view_top-STATUS_H):0;
                if (g_scroll>maxscroll) g_scroll=maxscroll;
                /* UI-24: apply shell zoom by scaling the destination rect */
                int draw_w = (int)(rw * g_zoom);
                int draw_h = (int)(rh * g_zoom);
                SDL_Rect src={0,g_scroll,rw,rh}; SDL_Rect dst={0,view_top,draw_w,draw_h};
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
            int tx = 0;
            for (size_t i=0;i<wuos_tb_count;i++){
                const WuosTbBtn *b = &wuos_tb_buttons[i];
                if (!b->label){ /* separator rule */
                    SDL_SetRenderDrawColor(ren, bd.r, bd.g, bd.b, 255);
                    SDL_RenderFillRect(ren, &(SDL_Rect){tx, ty+5, 1, TOOLBAR_H-10});
                    tx += 10;
                    continue;
                }
                int bw = wu_text_w(b->label) + 16;   /* REAL text width + comfy pad */
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
                    /* hover/press: solid accent fill + 1px white border; text
                     * flips to white (contrast on accent {96,140,255} >= 4.5:1). */
                    SDL_SetRenderDrawColor(ren, ac.r, ac.g, ac.b, press ? 255 : 235);
                    SDL_RenderFillRect(ren, &(SDL_Rect){tx, ty+3, bw, TOOLBAR_H-6});
                    SDL_SetRenderDrawColor(ren, 255, 255, 255, 130);
                    SDL_RenderDrawRect(ren, &(SDL_Rect){tx, ty+3, bw, TOOLBAR_H-6});
                    sdl_text(ren, tx + (bw - wu_text_w(b->label))/2,
                             ty + center_text_y(TOOLBAR_H), 255, 255, 255, b->label);
                } else {
                    /* resting: KEEP the bar surface (text stays high-contrast on
                     * the dark bar), but draw a clearly visible 1px border so the
                     * target unmistakably reads as a button (affordance via
                     * outline, not a light fill that would crush text contrast). */
                    SDL_SetRenderDrawColor(ren, bd.r+40, bd.g+40, bd.b+44, 230);
                    SDL_RenderDrawRect(ren, &(SDL_Rect){tx+1, ty+4, bw-2, TOOLBAR_H-8});
                }
                if (!(hov||press))
                    sdl_text(ren, tx + (bw - wu_text_w(b->label))/2,
                             ty + center_text_y(TOOLBAR_H),
                             ttx.r, ttx.g, ttx.b, b->label);
                tx += bw + 4;
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
                        /* clamp the entry to the panel width: truncate + ellipsis
                         * so long titles never run off the right edge (UX: a
                         * sidebar row must stay inside its panel). */
                        int maxw = SIDEBAR_W - 16;
                        char buf[96]; if (L>95) L=95;
                        memcpy(buf, line, L); buf[L]=0;
                        if (wu_text_w(buf) > maxw && L > 1){
                            int cut = L;
                            while (cut > 1 && wu_text_w(buf) > maxw) buf[--cut]=0;
                            int el = cut<3?cut:cut-3;
                            buf[el]='.'; buf[el+1]='.'; buf[el+2]='.';
                            buf[el+3]=0;
                        }
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
