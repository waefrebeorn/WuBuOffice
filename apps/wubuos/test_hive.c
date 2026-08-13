/* test_hive.c — verify the data-driven hive template parses and exposes the
 * expected toolbar / menus / slide. A regression guard against hardcoding:
 * if someone hand-edits the embedded default, the app's UI structure changed
 * and this catches it. Also validates a custom JSON template round-trips. */
#include "hive.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

int main(void){
    Hive *h = hive_load();
    CK(h, "hive_load");

    /* toolbar: the default template has the 4-command File group at the top */
    const HiveToolbar *tb = hive_toolbar(h);
    CK(tb && tb->n > 0, "toolbar present");
    if (tb){
        CK(tb->items[0].label && !strcmp(tb->items[0].label, "New"), "toolbar[0]=New");
        /* separators present (label NULL) */
        int seps = 0;
        for (size_t i = 0; i < tb->n; i++) if (!tb->items[i].label) seps++;
        CK(seps >= 4, "toolbar has group separators");
        /* every non-sep has a cmd */
        int cmds = 0;
        for (size_t i = 0; i < tb->n; i++) if (tb->items[i].label && tb->items[i].cmd) cmds++;
        CK(cmds >= 15, "toolbar buttons carry cmds");
    }

    /* menus: File/Edit/View/Help with accelerators */
    size_t nmenus = 0;
    const HiveMenu *menus = hive_menus(h, &nmenus);
    CK(menus && nmenus == 4, "4 menus");
    if (menus && nmenus >= 4){
        CK(!strcmp(menus[0].label, "File"), "menu[0]=File");
        CK(menus[0].n >= 10, "File has items");
        /* File[0]=Open with accel */
        CK(menus[0].items[0].accel && !strcmp(menus[0].items[0].accel, "Ctrl+O"), "File/Open accel");
        /* View menu has zoom */
        CK(!strcmp(menus[2].label, "View"), "menu[2]=View");
    }

    /* slide: title + bullets + chart */
    const HiveSlide *sl = hive_slide(h);
    CK(sl && sl->title, "slide title");
    CK(sl && sl->nbullets >= 3, "slide has bullets");
    CK(sl && sl->nchart >= 3, "slide has chart values");
    if (sl){
        CK(strstr(sl->title, "Slide") != NULL, "slide title mentions slide");
        CK(sl->chart[0] > 0, "chart has positive value");
    }

    hive_free(h);

    /* ---- custom template via env: point at a temp file ---- */
    {
        FILE *f = fopen("/tmp/wubuos_hive_test.json", "w");
        CK(f, "open temp hive");
        if (f){
            fprintf(f, "{\"toolbar\":[{\"label\":\"Alpha\",\"cmd\":9001},"
                       "{\"sep\":true},{\"label\":\"Beta\",\"cmd\":9002}],"
                       "\"menus\":[],\"slide\":{\"title\":\"T\",\"bullets\":[\"x\"],\"chart\":[1,2]}}\n");
            fclose(f);
        }
        const char *old = getenv("WUBU_HIVE");
        setenv("WUBU_HIVE", "/tmp/wubuos_hive_test.json", 1);
        Hive *h2 = hive_load();
        const HiveToolbar *tb2 = hive_toolbar(h2);
        CK(tb2 && tb2->n == 3 && tb2->items[0].label && !strcmp(tb2->items[0].label, "Alpha"),
           "custom toolbar overrides default");
        const HiveSlide *sl2 = hive_slide(h2);
        CK(sl2 && sl2->nbullets == 1, "custom slide bullets");
        hive_free(h2);
        if (old) setenv("WUBU_HIVE", old, 1); else unsetenv("WUBU_HIVE");
        remove("/tmp/wubuos_hive_test.json");
    }

    if (fails == 0) printf("HIVE CONFIG TESTS PASSED\n");
    else printf("HIVE CONFIG TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
