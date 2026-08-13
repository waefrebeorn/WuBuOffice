/* hive.c — data-driven UI configuration ("the hive"). Loads a declarative
 * JSON template (toolbar / menus / slide) at startup and exposes typed views.
 * An embedded default template runs with no external file; a user template at
 * ~/.config/wubuos/hive.json (or $WUBU_HIVE) overrides/extends it. */
#include "hive.h"
#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Hive {
    HiveToolbar tb;
    HiveMenu   *menus;
    size_t      nmenus;
    HiveSlide   slide;
};

/* ---- default template (embedded; matches the former static arrays) ---- */
static const char *kDefaultTemplate =
"{"
" \"toolbar\": ["
"  {\"label\":\"New\",\"cmd\":1003},{\"label\":\"Open\",\"cmd\":1000},"
"  {\"label\":\"Save\",\"cmd\":1001},{\"label\":\"Save As\",\"cmd\":1002},"
"  {\"sep\":true},"
"  {\"label\":\"Undo\",\"cmd\":1005},{\"label\":\"Redo\",\"cmd\":1006},"
"  {\"sep\":true},"
"  {\"label\":\"Find\",\"cmd\":1007},{\"label\":\"Replace\",\"cmd\":1008},"
"  {\"sep\":true},"
"  {\"label\":\"H1\",\"cmd\":11},{\"label\":\"H2\",\"cmd\":12},{\"label\":\"H3\",\"cmd\":13},"
"  {\"label\":\"Body\",\"cmd\":14},{\"label\":\"Quote\",\"cmd\":15},{\"label\":\"Code\",\"cmd\":16},"
"  {\"sep\":true},"
"  {\"label\":\"Link\",\"cmd\":17},{\"label\":\"List\",\"cmd\":18},"
"  {\"label\":\"Table\",\"cmd\":19},{\"label\":\"Image\",\"cmd\":20},"
"  {\"sep\":true},"
"  {\"label\":\"PDF\",\"cmd\":1030},{\"label\":\"EPUB\",\"cmd\":1004}"
" ],"
" \"menus\": ["
"  {\"label\":\"File\",\"items\":["
"   {\"label\":\"Open...\",\"cmd\":1000,\"accel\":\"Ctrl+O\"},"
"   {\"label\":\"Save\",\"cmd\":1001,\"accel\":\"Ctrl+S\"},"
"   {\"label\":\"Save As...\",\"cmd\":1002,\"accel\":\"Ctrl+Shift+S\"},"
"   {\"label\":\"New Document\",\"cmd\":1003,\"accel\":\"Ctrl+T\"},"
"   {\"label\":\"Close Tab\",\"cmd\":1027,\"accel\":\"Ctrl+W\"},"
"   {\"sep\":true},"
"   {\"label\":\"Export EPUB\",\"cmd\":1004},{\"label\":\"Export PDF\",\"cmd\":1030},"
"   {\"label\":\"Export HTML\",\"cmd\":1031},{\"label\":\"Export Markdown\",\"cmd\":1032},"
"   {\"label\":\"Export LaTeX\",\"cmd\":1033},{\"label\":\"Export RTF\",\"cmd\":1034}"
"  ]},"
"  {\"label\":\"Edit\",\"items\":["
"   {\"label\":\"Undo\",\"cmd\":1005,\"accel\":\"Ctrl+Z\"},"
"   {\"label\":\"Redo\",\"cmd\":1006,\"accel\":\"Ctrl+Y\"},"
"   {\"label\":\"Find...\",\"cmd\":1007,\"accel\":\"Ctrl+F\"},"
"   {\"label\":\"Replace...\",\"cmd\":1008,\"accel\":\"Ctrl+H\"},"
"   {\"label\":\"Go to Line...\",\"cmd\":1009,\"accel\":\"Ctrl+G\"},"
"   {\"label\":\"Toggle Theme\",\"cmd\":1010},"
"   {\"sep\":true},"
"   {\"label\":\"Cut\",\"cmd\":1018,\"accel\":\"Ctrl+X\"},"
"   {\"label\":\"Copy\",\"cmd\":1019,\"accel\":\"Ctrl+C\"},"
"   {\"label\":\"Paste\",\"cmd\":1020,\"accel\":\"Ctrl+V\"},"
"   {\"label\":\"Paste Plain\",\"cmd\":1021,\"accel\":\"Ctrl+Shift+V\"},"
"   {\"label\":\"Select All\",\"cmd\":1022,\"accel\":\"Ctrl+A\"}"
"  ]},"
"  {\"label\":\"View\",\"items\":["
"   {\"label\":\"Zoom In\",\"cmd\":1011,\"accel\":\"Ctrl+=\"},"
"   {\"label\":\"Zoom Out\",\"cmd\":1012,\"accel\":\"Ctrl+-\"},"
"   {\"label\":\"Zoom Reset\",\"cmd\":1013,\"accel\":\"Ctrl+0\"},"
"   {\"label\":\"Word Wrap\",\"cmd\":1014},{\"label\":\"High Contrast\",\"cmd\":1015},"
"   {\"label\":\"Presentation\",\"cmd\":1024}"
"  ]},"
"  {\"label\":\"Help\",\"items\":["
"   {\"label\":\"Shortcuts\",\"cmd\":1016},{\"label\":\"First-run Tour\",\"cmd\":1017},"
"   {\"label\":\"About\",\"cmd\":1025}"
"  ]}"
" ],"
" \"slide\": {"
"  \"title\":\"WuBuOffice — Slide Deck\","
"  \"bullets\":["
"   \"One engine, every format: docx / xlsx / pptx / odt / pdf\","
"   \"Real OCR with a from-scratch recognizer\","
"   \"Notepad++-class editor embedded in the shell\","
"   \"All rendered through one shared surface\""
"  ],"
"  \"chart\":[40,65,50,80,55]"
" }"
"}";

/* ---- parsing helpers ---- */
static HiveItem *parse_items(const JVal *arr, size_t *nout){
    size_t n = j_len(arr);
    HiveItem *items = calloc(n ? n : 1, sizeof *items);
    if (!items) return NULL;
    for (size_t i = 0; i < n; i++){
        const JVal *it = j_arr_at(arr, i);
        if (!it) continue;
        if (j_type(it) == J_OBJ && j_obj_get(it, "sep")){
            items[i].label = NULL; items[i].cmd = 0;
            continue;
        }
        const JVal *lbl = j_obj_get(it, "label");
        const JVal *cmd = j_obj_get(it, "cmd");
        const JVal *acc = j_obj_get(it, "accel");
        items[i].label = (lbl && j_type(lbl)==J_STR) ? strdup(j_as_str(lbl)) : NULL;
        items[i].cmd   = (cmd && j_type(cmd)==J_NUM) ? (int)j_as_num(cmd) : 0;
        items[i].accel = (acc && j_type(acc)==J_STR) ? strdup(j_as_str(acc)) : NULL;
    }
    *nout = n;
    return items;
}

static void load_hive(Hive *h, const JVal *root){
    if (!root) return;
    /* toolbar */
    const JVal *tb = j_obj_get(root, "toolbar");
    if (tb && j_type(tb)==J_ARR){
        for (size_t i = 0; i < h->tb.n; i++){
            free((void*)h->tb.items[i].label);
            free((void*)h->tb.items[i].accel);
        }
        free(h->tb.items);
        h->tb.items = parse_items(tb, &h->tb.n);
    }
    /* menus */
    const JVal *menus = j_obj_get(root, "menus");
    if (menus && j_type(menus)==J_ARR){
        for (size_t i = 0; i < h->nmenus; i++){
            free((void*)h->menus[i].label);
            for (size_t k = 0; k < h->menus[i].n; k++){
                free((void*)h->menus[i].items[k].label);
                free((void*)h->menus[i].items[k].accel);
            }
            free(h->menus[i].items);
        }
        free(h->menus); h->menus = NULL; h->nmenus = 0;
        size_t n = j_len(menus);
        h->menus = calloc(n ? n : 1, sizeof *h->menus);
        for (size_t i = 0; i < n && h->menus; i++){
            const JVal *m = j_arr_at(menus, i);
            const JVal *lbl = j_obj_get(m, "label");
            const JVal *its = j_obj_get(m, "items");
            h->menus[i].label = (lbl && j_type(lbl)==J_STR) ? strdup(j_as_str(lbl)) : strdup("?");
            h->menus[i].items = (its && j_type(its)==J_ARR) ? parse_items(its, &h->menus[i].n) : NULL;
            h->nmenus++;
        }
    }
    /* slide */
    const JVal *sl = j_obj_get(root, "slide");
    if (sl && j_type(sl)==J_OBJ){
        const JVal *t = j_obj_get(sl, "title");
        if (t && j_type(t)==J_STR){ free(h->slide.title); h->slide.title = strdup(j_as_str(t)); }
        const JVal *bl = j_obj_get(sl, "bullets");
        if (bl && j_type(bl)==J_ARR){
            for (size_t i = 0; i < h->slide.nbullets; i++) free(h->slide.bullets[i]);
            free(h->slide.bullets);
            size_t n = j_len(bl);
            h->slide.bullets = calloc(n ? n : 1, sizeof *h->slide.bullets);
            h->slide.nbullets = 0;
            for (size_t i = 0; i < n; i++){
                const JVal *b = j_arr_at(bl, i);
                if (b && j_type(b)==J_STR) h->slide.bullets[h->slide.nbullets++] = strdup(j_as_str(b));
            }
        }
        const JVal *ch = j_obj_get(sl, "chart");
        if (ch && j_type(ch)==J_ARR){
            free(h->slide.chart);
            size_t n = j_len(ch);
            h->slide.chart = calloc(n ? n : 1, sizeof *h->slide.chart);
            h->slide.nchart = 0;
            for (size_t i = 0; i < n; i++){
                const JVal *v = j_arr_at(ch, i);
                if (v && j_type(v)==J_NUM) h->slide.chart[h->slide.nchart++] = j_as_num(v);
            }
        }
    }
}

/* ---- public API ---- */
Hive *hive_load(void){
    Hive *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    /* parse the embedded default first */
    JVal *root = j_parse(kDefaultTemplate, NULL);
    if (root){ load_hive(h, root); j_free(root); }
    /* user override: $WUBU_HIVE, else ~/.config/wubuos/hive.json */
    const char *path = getenv("WUBU_HIVE");
    char buf[1024];
    if (!path){
        const char *home = getenv("HOME");
        if (home) snprintf(buf, sizeof buf, "%s/.config/wubuos/hive.json", home);
        path = buf;
    }
    FILE *f = path ? fopen(path, "rb") : NULL;
    if (f){
        long sz; fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < (1<<20)){
            char *txt = malloc((size_t)sz + 1);
            if (txt){ size_t r = fread(txt, 1, (size_t)sz, f); txt[r] = 0;
                      JVal *ur = j_parse(txt, NULL);
                      if (ur){ load_hive(h, ur); j_free(ur); }
                      free(txt); }
        }
        fclose(f);
    }
    return h;
}

void hive_free(Hive *h){
    if (!h) return;
    for (size_t i = 0; i < h->tb.n; i++){ free((void*)h->tb.items[i].label); free((void*)h->tb.items[i].accel); }
    free(h->tb.items);
    for (size_t m = 0; m < h->nmenus; m++){
        free((void*)h->menus[m].label);
        for (size_t i = 0; i < h->menus[m].n; i++){ free((void*)h->menus[m].items[i].label); free((void*)h->menus[m].items[i].accel); }
        free(h->menus[m].items);
    }
    free(h->menus);
    free(h->slide.title);
    for (size_t i = 0; i < h->slide.nbullets; i++) free(h->slide.bullets[i]);
    free(h->slide.bullets);
    free(h->slide.chart);
    free(h);
}

const HiveToolbar *hive_toolbar(const Hive *h){ return h ? &h->tb : NULL; }
const HiveMenu *hive_menus(const Hive *h, size_t *n){ if (n) *n = h ? h->nmenus : 0; return h ? h->menus : NULL; }
const HiveSlide *hive_slide(const Hive *h){ return h ? &h->slide : NULL; }
