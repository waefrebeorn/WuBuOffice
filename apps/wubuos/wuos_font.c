/* wuos_font.c -- shared FreeType text raster helper (see wuos_font.h). */
#include "wuos_font.h"
#include "wububase.h"   /* utf8 decode */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

static FT_Library g_ft;
static FT_Face    g_reg, g_bold;
static int        g_size = 20;
static int        g_inited = 0;
static int        g_family = -1;      /* INT-15: active family index */

/* INT-15: enumerated font families (regular + bold path per family) */
#define WF_MAX_FAM 256
#define WF_PATH 1024
typedef struct { char family[64]; char reg[WF_PATH]; char bold[WF_PATH]; } FontFam;
static FontFam g_fams[WF_MAX_FAM];
static int     g_nfam = 0;

/* read a font file's family name via FreeType; returns 1 if it is a usable
 * outline face, fills fam[] (family) and is_bold. */
static int wf_probe(const char *path, char *fam, int famsz, int *is_bold){
    FT_Face f = NULL;
    if (FT_New_Face(g_ft, path, 0, &f) != 0) return 0;
    if (!f->family_name){ FT_Done_Face(f); return 0; }
    snprintf(fam, famsz, "%s", f->family_name);
    *is_bold = (f->style_name && strstr(f->style_name, "Bold")) ? 1 : 0;
    FT_Done_Face(f);
    return 1;
}

static void wf_add(const char *fam, const char *path, int is_bold){
    if (g_nfam >= WF_MAX_FAM) return;
    /* find existing family (case-insensitive) */
    for (int i=0;i<g_nfam;i++){
        if (!strcasecmp(g_fams[i].family, fam)){
            if (is_bold){ if (!g_fams[i].bold[0]) snprintf(g_fams[i].bold, WF_PATH, "%s", path); }
            else        { if (!g_fams[i].reg[0])  snprintf(g_fams[i].reg,  WF_PATH, "%s", path); }
            return;
        }
    }
    snprintf(g_fams[g_nfam].family, sizeof g_fams[g_nfam].family, "%s", fam);
    if (is_bold) snprintf(g_fams[g_nfam].bold, WF_PATH, "%s", path);
    else         snprintf(g_fams[g_nfam].reg,  WF_PATH, "%s", path);
    g_nfam++;
}

static void wf_scan_dir(const char *dir){
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))){
        const char *nm = e->d_name;
        char full[WF_PATH];
        snprintf(full, sizeof full, "%s/%s", dir, nm);
        /* descend into subdirs */
        struct stat st;
        if (!stat(full, &st)){
            if (S_ISDIR(st.st_mode)){
                if (strcmp(nm, ".") && strcmp(nm, "..")) wf_scan_dir(full);
                continue;
            }
        }
        /* only outline font extensions */
        size_t L = strlen(nm);
        if (L < 5) continue;
        if (strcasecmp(nm+L-4, ".ttf") && strcasecmp(nm+L-4, ".otf") &&
            strcasecmp(nm+L-4, ".ttc")) continue;
        char fam[64]; int bold=0;
        if (wf_probe(full, fam, sizeof fam, &bold)) wf_add(fam, full, bold);
    }
    closedir(d);
}

void wuos_font_scan(void){
    if (!g_inited) return;
    const char *home = getenv("HOME");
    const char *dirs[] = {
        "/usr/share/fonts", "/usr/local/share/fonts",
        home ? "/.fonts" : NULL,
        home ? "/.local/share/fonts" : NULL,
        NULL };
    for (int i=0; dirs[i]; i++){
        char buf[WF_PATH];
        if (dirs[i][0] == '~') snprintf(buf, sizeof buf, "%s%s", home?home:"", dirs[i]+1);
        else snprintf(buf, sizeof buf, "%s", dirs[i]);
        wf_scan_dir(buf);
    }
    /* guarantee at least the fallback faces are present */
    if (g_nfam == 0){
        snprintf(g_fams[0].family, sizeof g_fams[0].family, "Default");
        snprintf(g_fams[0].reg,  WF_PATH, "%s", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        snprintf(g_fams[0].bold, WF_PATH, "%s", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
        g_nfam = 1;
    }
}

int wuos_font_family_count(void){ return g_nfam; }
const char *wuos_font_family_name(int i){
    return (i>=0 && i<g_nfam) ? g_fams[i].family : "";
}
int wuos_font_current_family(void){ return g_family; }

int wuos_font_set_family(int i){
    if (!g_inited || i<0 || i>=g_nfam) return -1;
    if (i == g_family) return 0;
    FT_Face nr = NULL, nb = NULL;
    if (FT_New_Face(g_ft, g_fams[i].reg, 0, &nr) != 0) return -1;
    if (g_fams[i].bold[0]){
        if (FT_New_Face(g_ft, g_fams[i].bold, 0, &nb) != 0) nb = NULL;
    }
    if (!nb) nb = nr;   /* fall back to regular if no bold face */
    /* swap in: free old faces (avoid freeing nb if it aliases nr) */
    if (g_reg && g_reg != g_bold) FT_Done_Face(g_reg);
    else if (g_reg && g_reg == g_bold) FT_Done_Face(g_reg);
    if (g_bold && g_bold != g_reg) FT_Done_Face(g_bold);
    g_reg = nr; g_bold = nb;
    FT_Set_Pixel_Sizes(g_reg, 0, (FT_UInt)g_size);
    FT_Set_Pixel_Sizes(g_bold,0, (FT_UInt)g_size);
    g_family = i;
    return 0;
}

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
    /* INT-15: enumerate families and resolve which one the default faces are */
    wuos_font_scan();
    g_family = -1;
    for (int i=0;i<g_nfam;i++){
        if (!strcmp(g_fams[i].reg, cands[0]) || !strcmp(g_fams[i].reg, cands[1]) ||
            !strcmp(g_fams[i].reg, cands[2])){ g_family = i; break; }
    }
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

int wuos_font_draw_s(const char *s, int x, int y, int bold, int size,
                     unsigned char r, unsigned char g, unsigned char b,
                     unsigned char *fb, int fbw, int fbh){
    if (size < 6) size = 6; if (size > 96) size = 96;
    FT_Face face = (bold && g_bold)? g_bold : g_reg;
    if (!face || !fb) return 0;
    int prev = g_size;
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)size);
    g_size = size;
    int adv = wuos_font_draw(s, x, y, bold, r, g, b, fb, fbw, fbh);
    /* restore */
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)prev);
    g_size = prev;
    return adv;
}

/* svg_text_fn adapter: size is in position 4 (not bold). Render with the
 * sized variant at the requested size. */
void wuos_svg_text(const char *s, int x, int y, int size,
                   unsigned char r, unsigned char g, unsigned char b,
                   unsigned char *fb, int fbw, int fbh){
    if (!s || !fb) return;
    wuos_font_draw_s(s, x, y, 0, size, r, g, b, fb, fbw, fbh);
}
