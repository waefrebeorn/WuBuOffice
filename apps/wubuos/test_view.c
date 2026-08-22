/* Headless smoke test: create + render every view (real engines, no GUI).
 * Also exercises file open (doc markdown + editor code) and Ctrl+S save. */
#include "wuos.h"
#include "wuos_font.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"     /* DOC-42: command-palette keyboard-nav test */
#include "wuos_file.h"
#include "autosave.h"   /* wubuautosave: editor crash-recovery test */
#include "model.h"      /* wubumodel_doc: build snapshot in autosave test */
#include "cell.h"       /* wubucell: spreadsheet round-trip save check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int render_check(WuView *v, const char *name){
    unsigned char *rgba=NULL; int w=0,h=0;
    int rc = v->render(v, 960, 664, 0, &rgba, &w, &h);
    if (rc!=0 || !rgba){ fprintf(stderr,"[%s] render FAILED rc=%d\n", name, rc); free(rgba); return 1; }
    fprintf(stderr,"[%s] ok %dx%d\n", name, w, h);
    free(rgba);
    /* status bar text must be produced (the live shell paints it) */
    char *st = v->status? v->status(v): NULL;
    if (st){
        if (st[0]=='\0'){ fprintf(stderr,"[%s] status empty\n", name); free(st); return 1; }
        free(st);
    }
    /* tab label must be measurable text (the live shell paints it) */
    static unsigned char scratch[8*64*4];
    int adv = wuos_font_draw(name, 0, 0, 0, 0,0,0, scratch, 8, 64);
    if (adv <= 0){ fprintf(stderr,"[%s] tab label unmeasurable\n", name); return 1; }
    return 0;
}

int main(void){
    if (wuos_font_init()!=0){ fprintf(stderr,"font init FAILED\n"); return 2; }

    /* no-path views */
    struct { const char *name; WuView *(*mk)(const char*); } tbl[] = {
        {"doc", wuos_doc_create},
        {"cell", wuos_cell_create},
        {"slide", wuos_slide_create},
        {"ocr",  wuos_ocr_create},
        {"editor", wuos_editor_create},
        {NULL,NULL}
    };
    int bad=0;
    for (int i=0; tbl[i].name; i++){
        WuView *v = tbl[i].mk(NULL);
        if (!v){ fprintf(stderr,"[%s] create FAILED\n", tbl[i].name); bad++; continue; }
        bad += render_check(v, tbl[i].name);
        /* Navigator sidebar content callback must be safe to call for every
         * view (may return NULL when the view has no structure). */
        if (v->sidebar){
            char *sb = v->sidebar(v);
            if (sb && !sb[0]){ fprintf(stderr,"[%s] sidebar empty string\n", tbl[i].name); bad++; }
            free(sb);
        }
        v->destroy(v);
    }

    /* ---- SLIDE round-trip: create an editable slide, edit title/bullets via
     * on_key, save, reload, verify content survives (was a static stub). */
    {
        WuView *sv = wuos_slide_create(NULL);
        if (!sv){ fprintf(stderr,"[slide rt] create FAILED\n"); bad++; }
        else {
            if (!sv->save || !sv->on_key){ fprintf(stderr,"[slide rt] missing save/on_key hook\n"); bad++; }
            else {
                /* edit the title (Enter starts editing sel=0 title) */
                sv->on_key(sv, WUOS_KEY_RETURN, 1);
                for (const char*p="MyDeck";*p;p++) sv->on_key(sv,(unsigned char)*p,1);
                sv->on_key(sv, WUOS_KEY_ESC, 1);
                /* Enter adds+edits a bullet */
                sv->on_key(sv, WUOS_KEY_RETURN, 1);
                for (const char*p="Hello";*p;p++) sv->on_key(sv,(unsigned char)*p,1);
                sv->on_key(sv, WUOS_KEY_ESC, 1);
                sv->save(sv);   /* writes /tmp/wubuos_slide.txt */
                /* verify the saved file contains both edits */
                FILE *f = fopen("/tmp/wubuos_slide.txt","r");
                if (!f){ fprintf(stderr,"[slide rt] save produced no file\n"); bad++; }
                else {
                    char buf[512] = {0};
                    size_t r = fread(buf,1,sizeof buf-1,f); buf[r]=0; fclose(f);
                    if (strstr(buf,"MyDeck") && strstr(buf,"Hello"))
                        fprintf(stderr,"[slide rt] edit->save OK (title+bullet persisted)\n");
                    else { fprintf(stderr,"[slide rt] edits not in saved file\n"); bad++; }
                }
                /* reload via create(path) */
                WuView *sv2 = wuos_slide_create("/tmp/wubuos_slide.txt");
                if (!sv2){ fprintf(stderr,"[slide rt] reload create FAILED\n"); bad++; }
                else { fprintf(stderr,"[slide rt] reload OK (path=%s)\n",
                               sv2->get_path? sv2->get_path(sv2):"?"); sv2->destroy(sv2); }
            }
            sv->destroy(sv);
        }
    }

    bad += testview_docfile();
    bad += testview_editor();
    bad += testview_cell();
    bad += testview_ocr_doc();

    wuos_font_quit();
    fprintf(stderr,"done bad=%d\n", bad);
    return bad?1:0;
}
