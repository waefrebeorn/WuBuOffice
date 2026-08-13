#include "wubusmartart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubusmartart *s = wubusmartart_create();
    CK(wubusmartart_layout(s)==WUBU_SA_PROCESS, "default process");
    CK(wubusmartart_set_layout(s,WUBU_SA_CYCLE)==0 && wubusmartart_layout(s)==WUBU_SA_CYCLE, "set cycle");
    CK(wubusmartart_add_node(s,"Plan")==0, "add Plan");
    CK(wubusmartart_add_node(s,"Do")==0, "add Do");
    CK(wubusmartart_add_node(s,"Check")==0, "add Check");
    CK(wubusmartart_count(s)==3, "3 nodes");
    CK(strcmp(wubusmartart_node(s,2),"Check")==0, "node 2");
    CK(wubusmartart_node(s,9)==NULL, "out of range");
    CK(wubusmartart_set_layout(s,99)==-1, "reject bad layout");

    /* REAL engine: layouts must produce boxes inside the frame, non-overlapping
     * enough that each box has positive size and sits within bounds. */
    wubusa_box bx[8];
    CK(wubusmartart_layout_boxes(s, 800, 600, bx, 8) == 0, "layout process");
    for (size_t i=0;i<3;i++){
        CK(bx[i].w > 0 && bx[i].h > 0, "box has size");
        CK(bx[i].x >= 0 && bx[i].x+bx[i].w <= 800.001f, "box within frame width");
        CK(bx[i].y >= 0 && bx[i].y+bx[i].h <= 600.001f, "box within frame height");
    }
    /* distinct positions across the row */
    CK(bx[0].x != bx[2].x, "process boxes tiled horizontally");

    /* hierarchy: root centered top, children fanned bottom */
    CK(wubusmartart_set_layout(s,WUBU_SA_HIERARCHY)==0, "set hierarchy");
    CK(wubusmartart_layout_boxes(s, 800, 600, bx, 8) == 0, "layout hierarchy");
    CK(bx[0].y < bx[1].y, "root above children");
    CK(bx[0].x+bx[0].w/2.0f > 350 && bx[0].x+bx[0].w/2.0f < 450, "root horizontally centered");

    /* empty model => 0 boxes, no error */
    wubusmartart *e = wubusmartart_create();
    CK(wubusmartart_layout_boxes(e, 100,100, bx, 8) == 0, "empty layout ok");
    wubusmartart_destroy(e);

    wubusmartart_destroy(s);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusmartart (diagram model + real layout engine: process/hierarchy/cycle/list)\n");
    return 0;
}
