/* throwaway: create + render each view headlessly to confirm no crash */
#include "wuos.h"
#include "wuos_font.h"
#include <stdio.h>
#include <stdlib.h>

int main(void){
    if (wuos_font_init()!=0){ fprintf(stderr,"font init FAILED\n"); return 2; }
    struct { const char *name; WuView *(*mk)(void); } tbl[] = {
        {"doc", wuos_doc_create},
        {"cell", wuos_cell_create},
        {"slide", wuos_slide_create},
        {"ocr", wuos_ocr_create},
        {"editor", wuos_editor_create},
        {NULL,NULL}
    };
    int bad=0;
    for (int i=0; tbl[i].name; i++){
        WuView *v = tbl[i].mk();
        if (!v){ fprintf(stderr,"[%s] create FAILED\n", tbl[i].name); bad++; continue; }
        unsigned char *rgba=NULL; int w=0,h=0;
        int rc = v->render(v, 960, 664, 0, &rgba, &w, &h);
        if (rc!=0 || !rgba){ fprintf(stderr,"[%s] render FAILED rc=%d\n", tbl[i].name, rc); bad++; }
        else fprintf(stderr,"[%s] ok %dx%d\n", tbl[i].name, w, h);
        free(rgba);
        char *st = v->status? v->status(v): NULL;
        if (st) free(st);
        v->destroy(v);
    }
    wuos_font_quit();
    fprintf(stderr,"done bad=%d\n", bad);
    return bad?1:0;
}
