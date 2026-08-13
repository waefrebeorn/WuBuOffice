/* test_wuos_toolbar.c -- headless checks for the shell's formatting toolbar
 * (wuos_toolbar). Verifies the button table, hit-testing round-trips, command
 * mapping, and that the strip fits the shell window width. The toolbar is now
 * data-driven (built from the hive template), so the test builds it from the
 * hive first. */
#include "wuos_toolbar.h"
#include "wuos_font.h"
#include "hive.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

static void test_table(void){
    CK(wuos_tb_count > 0, "button table non-empty");
    CK(wuos_tb_label_count() == 15, "15 clickable buttons");
    /* every label button has a command */
    int allcmds = 1;
    for (size_t i=0;i<wuos_tb_count;i++)
        if (wuos_tb_buttons[i].label && wuos_tb_buttons[i].cmd==0) allcmds=0;
    CK(allcmds, "all buttons dispatch a command");
}

static void test_hit_testing(void){
    /* walk the layout exactly as wuos_tb_at computes it; each button's center
     * must map back to itself, and the whole strip must round-trip */
    int x=0;
    for (size_t i=0;i<wuos_tb_count;i++){
        if (!wuos_tb_buttons[i].label){ x += 10; continue; }
        /* compute width the same way (REAL font width + pad) */
        int bw = wuos_font_text_width(wuos_tb_buttons[i].label, wuos_font_height()) + 10;
        int c = x + bw/2;
        int idx = wuos_tb_at(c);
        if (idx != (int)i){
            printf("FAIL: roundtrip %zu: center %d -> %d\n", i, c, idx); fails++;
        }
        x += bw + 1;
    }
    /* just past the last button -> none */
    CK(wuos_tb_at(x) == -1, "past end hits nothing");
    CK(wuos_tb_at(-5) == -1, "negative hits nothing");
    CK(wuos_tb_at(0) == 0, "first button at x=0");
}

static void test_fits_window(void){
    /* shell window is 960px wide (WIN_W in main.c); toolbar must fit */
    CK(wuos_tb_width() < 960, "toolbar fits in 960px window");
    printf("toolbar spans %d px\n", wuos_tb_width());
}

static void test_command_mapping(void){
    CK(wuos_tb_cmd_to_key(11) != 0, "H1 maps to a key");
    CK(wuos_tb_cmd_to_key(12) != 0, "H2 maps to a key");
    CK(wuos_tb_cmd_to_key(20) != 0, "Image maps to a key");
    CK(wuos_tb_cmd_to_key(1003) == 0, "menu command is not a key");
    CK(wuos_tb_cmd_to_key(0) == 0, "0 maps to nothing");
    /* distinct keys for distinct formatting buttons */
    CK(wuos_tb_cmd_to_key(11) != wuos_tb_cmd_to_key(12), "H1 != H2");
}

int main(void){
    /* the toolbar is data-driven: build it from the hive template */
    if (wuos_font_init()!=0){ printf("FAIL: font init\n"); return 1; }
    Hive *h = hive_load();
    CK(h, "hive_load");
    wuos_tb_init(hive_toolbar(h));

    test_table();
    test_hit_testing();
    test_fits_window();
    test_command_mapping();

    wuos_tb_shutdown();
    hive_free(h);
    wuos_font_quit();
    if (fails == 0) printf("TOOLBAR TESTS PASSED\n");
    else printf("TOOLBAR TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
