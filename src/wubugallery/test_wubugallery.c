#include "wubugallery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubugallery *g = wubugallery_create();
    CK(wubugallery_add_item(g,"Clipart","star.png") == 0, "add star");
    CK(wubugallery_add_item(g,"Clipart","heart.png") == 0, "add heart");
    CK(wubugallery_add_item(g,"Shapes","rect") == 0, "add rect");
    CK(wubugallery_count(g) == 2, "2 galleries");

    wubugallery_col *c = wubugallery_get(g,"Clipart");
    CK(c && c->n == 2 && strcmp(c->items[0],"star.png") == 0 && strcmp(c->items[1],"heart.png") == 0, "clipart items");
    CK(strcmp(wubugallery_name(g,0),"Clipart") == 0, "gallery 0 name");

    wubugallery_destroy(g);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubugallery (named collections, add/get items)\n");
    return 0;
}
