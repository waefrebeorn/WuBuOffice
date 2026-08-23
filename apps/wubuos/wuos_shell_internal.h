/* wuos_shell_internal.h -- shared shell state for the wubuos shell modules.
 * Internal only: not installed, not part of any public API. */
#ifndef WUOS_SHELL_INTERNAL_H
#define WUOS_SHELL_INTERNAL_H

#include <SDL2/SDL.h>
#include "wuos.h"

#include "wuos_theme.h"

/* Shell layout constants. */
#define WIN_W 960
#define WIN_H 720
#define TAB_H      ((((wuos_font_height()*3)/2) + 2) & ~3)   /* 32 @ fh20 */
#define MENU_H     ((wuos_font_height()+4) & ~3)             /* 24 @ fh20 */
#define TOOLBAR_H  (((wuos_font_height()+6) + 2) & ~3)       /* 28 @ fh20 */
#define SIDEBAR_W  (((wuos_font_height()*11) + 2) & ~3)      /* 224 @ fh20 */
#define STATUS_H   (((wuos_font_height()+6) + 2) & ~3)       /* 28 @ fh20 */

/* Shell state (defined in main.c; one instance per process). */
extern int g_scroll;
extern WuView *views[8];
extern int     nviews;
extern int     active;
extern int     tab_hover;
extern int     tab_drag_from;
extern int     tab_drag_x;
extern float   g_zoom;
extern int     g_zoom_drag;
extern int     g_sidebar;
extern int     g_ctx;
extern int     g_ctx_item;
extern int     g_ctx_x, g_ctx_y;
#include "toast.h"
#include "palette.h"
extern Toasts *g_toasts;
extern Palette *g_palette;
#include "wuos_motion.h"
extern WuosTween g_tab_ul;
extern int       g_tab_ul_from, g_tab_ul_to;
extern float     g_tb_press_t;
extern int       g_tb_press_i;
extern Uint32    g_caret_phase;
extern int      g_cheat;
extern int      g_first_run;
#include "dialog.h"

/* H4: shell accessibility tree (JSON) -- wuos_shell_a11y.c */
char *wuos_shell_a11y_tree(void);
extern Dialog *g_dlg;
extern int      g_dlg_action;
#include "hive.h"
extern Hive *g_hive;
extern const HiveMenu *g_menus;
extern size_t g_nmenus;
extern int g_menu_open;
extern int g_menu_hover;
extern int g_tb_hover;

#include "plugin.h"
extern WuOSPluginMgr *g_plugins;
extern char   *g_plugin_msg;
extern int     g_plugin_idx;

/* Shared helpers (main.c). */
void zoom_from_x(int x);
void apply_zoom(void);
void run_tb_cmd(int cmd);
void run_menu_cmd(int cmd);
void add_view(WuView *v);
int  tab_at(int mx);
int  tab_width(int i);
int  wu_text_w(const char *s);
void tab_reorder(int from, int to);
int  center_text_y(int band_h);
void sdl_text(SDL_Renderer *ren, int px, int py,
              unsigned char r, unsigned char g, unsigned char b,
              const char *text);

/* Render one full frame into ren (wuos_frame.c). */
void wuos_frame_render(SDL_Renderer *ren);

#endif /* WUOS_SHELL_INTERNAL_H */
