#include "wubunotebookbar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubunotebookbar *n = wubunotebookbar_create();
    CK(wubunotebookbar_add(n,"Sheet1")==0, "add Sheet1");
    CK(wubunotebookbar_add(n,"Sheet2")==0, "add Sheet2");
    CK(wubunotebookbar_add(n,"Sheet3")==0, "add Sheet3");
    CK(wubunotebookbar_count(n)==3, "3 tabs");
    CK(strcmp(wubunotebookbar_name(n,1),"Sheet2")==0, "tab 1 name");
    CK(wubunotebookbar_active(n)==0, "default active 0");
    CK(wubunotebookbar_set_active(n,2)==0 && wubunotebookbar_active(n)==2, "set active 2");
    CK(wubunotebookbar_set_active(n,9)==-1, "reject out of range");

    /* REAL engine: geometry a renderer draws + hit-tests. */
    double x, y, w, h;
    CK(wubunotebookbar_tab_rect(n, 1, 0.0, 700.0, 120.0, 24.0, &x, &y, &w, &h) == 0, "rect tab1");
    CK(x == 120.0, "tab1 x = index*width");          /* 1 * 120 */
    CK(y == 700.0 && w == 120.0 && h == 24.0, "tab rect dims");
    /* tab0 starts at x0 */
    double x0;
    wubunotebookbar_tab_rect(n, 0, 0.0, 700.0, 120.0, 24.0, &x0, &y, &w, &h);
    CK(x0 == 0.0, "tab0 at origin");
    CK(wubunotebookbar_tab_rect(n, 9, 0,0,120,24, &x, &y, &w, &h) == -1, "bad index");

    wubunotebookbar_destroy(n);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubunotebookbar (sheet-tab strip + drawable/hit-test rects)\n");
    return 0;
}
