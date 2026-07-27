/* wuos_font.c -- shared FreeType text raster helper (see wuos_font.h). */
#include "wuos_font.h"
#include "wububase.h"   /* utf8 decode */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>
#include <stdlib.h>
#include <math.h>

static FT_Library g_ft;
static FT_Face    g_reg, g_bold;
static int        g_size = 20;
static int        g_inited = 0;

int wuos_font_init(void){
    if (g_inited) return 0;
    if (FT_Init_FreeType(&g_ft)) return -1;
    const char *cands[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf", NULL };
    const char *cands_b[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", NULL };
    for (int i=0;cands[i];i++)   if (FT_New_Face(g_ft,cands[i],0,&g_reg)==0) break;
    for (int i=0;cands_b[i];i++) if (FT_New_Face(g_ft,cands_b[i],0,&g_bold)==0) break;
    if (!g_reg) return -1;
    if (!g_bold) g_bold = g_reg;
    FT_Set_Pixel_Sizes(g_reg, 0, (FT_UInt)g_size);
    FT_Set_Pixel_Sizes(g_bold,0, (FT_UInt)g_size);
    g_inited = 1;
    return 0;
}

void wuos_font_quit(void){
    if (!g_inited) return;
    if (g_reg  && g_reg  != g_bold) FT_Done_Face(g_reg);
    if (g_bold) FT_Done_Face(g_bold);
    FT_Done_FreeType(g_ft);
    g_inited = 0;
}

int wuos_font_height(void){ return g_size; }

/* Pixel width of `s` at the current font size (no rasterization). */
int wuos_font_text_width(const char *s, int size){
    FT_Face face = g_reg;  /* width uses the regular face */
    if (!face || !s) return 0;
    int ox = 0;
    const char *p = s;
    while (*p){
        uint32_t cp;
        int k = wububase_utf8_decode(p, &cp);
        if (k <= 0){ p++; continue; }
        p += k;
        if (FT_Load_Char(face, (FT_ULong)cp, FT_LOAD_DEFAULT)) continue;
        ox += (int)face->glyph->advance.x >> 6;
    }
    (void)size;  /* size is applied via the global font size; kept for API symmetry */
    return ox;
}

int wuos_font_draw(const char *s, int x, int y, int bold,
                   unsigned char r, unsigned char g, unsigned char b,
                   unsigned char *fb, int fbw, int fbh){
    FT_Face face = (bold && g_bold)? g_bold : g_reg;
    if (!face || !fb) return 0;
    int ox = x;
    const char *p = s;
    while (*p){
        uint32_t cp;
        int k = wububase_utf8_decode(p, &cp);
        if (k <= 0){ p++; continue; }
        p += k;
        if (FT_Load_Char(face, (FT_ULong)cp, FT_LOAD_RENDER)) continue;
        FT_GlyphSlot gl = face->glyph;
        int gx = ox + gl->bitmap_left;
        int gy = y - gl->bitmap_top;
        int pp = gl->bitmap.pitch;
        int abw = gl->bitmap.width;
        int rows = (int)gl->bitmap.rows;
        for (int row=0; row<rows; row++)
            for (int col=0; col<abw; col++){
                int src = (pp<0)? (rows-1-row)*(-pp)+col : row*pp+col;
                unsigned char a = gl->bitmap.buffer[src];
                if (a>20){
                    int xx=gx+col, yy=gy+row;
                    if (xx>=0&&yy>=0&&xx<fbw&&yy<fbh){
                        size_t i=((size_t)yy*fbw+xx)*4;
                        fb[i]   = (unsigned char)((r*a + fb[i]*(255-a))/255);
                        fb[i+1] = (unsigned char)((g*a + fb[i+1]*(255-a))/255);
                        fb[i+2] = (unsigned char)((b*a + fb[i+2]*(255-a))/255);
                        fb[i+3] = 255;
                    }
                }
            }
        ox += (int)gl->advance.x >> 6;
    }
    return ox - x;
}
