#include "wubusidebar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubusidebar *s = wubusidebar_create();
    CK(wubusidebar_add_panel(s,"Styles") == 0, "add Styles");
    CK(wubusidebar_add_panel(s,"Navigator") == 0, "add Navigator");
    CK(wubusidebar_add_panel(s,"Gallery") == 0, "add Gallery");
    CK(wubusidebar_count(s) == 3, "3 panels");

    CK(strcmp(wubusidebar_title(s,1),"Navigator") == 0, "panel 1 title");
    CK(wubusidebar_active(s) == 0, "default active 0");
    CK(wubusidebar_set_active(s,2) == 0 && wubusidebar_active(s) == 2, "set active 2");
    CK(wubusidebar_set_active(s,9) == -1, "out of range rejected");

    CK(wubusidebar_visible(s) == 1, "default visible");
    CK(wubusidebar_show(s,0) == 0 && wubusidebar_visible(s) == 0, "hide");
    CK(wubusidebar_show(s,1) == 0 && wubusidebar_visible(s) == 1, "show");

    wubusidebar_destroy(s);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubusidebar (named panels, active index, visibility)\n");
    return 0;
}
