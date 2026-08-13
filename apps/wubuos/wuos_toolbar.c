#include "wuos_toolbar.h"
#include "wuos.h"   /* WUOS_KEY_STYLE_* / WUOS_KEY_INSERT_* sentinels */
#include "hive.h"   /* data-driven toolbar template */
#include <stdlib.h>
#include <string.h>

/* Toolbar buttons. Data-driven from the hive template (wuos_tb_init) — NOT a
 * hardcoded static array. cmd >=1000 = menu/palette command; 11..20 =
 * formatting/insert forwarded to the active view's on_key. */
WuosTbBtn *wuos_tb_buttons = NULL;
size_t     wuos_tb_count = 0;

void wuos_tb_init(const HiveToolbar *tb){
    /* free any previous table */
    if (wuos_tb_buttons){ free(wuos_tb_buttons); wuos_tb_buttons = NULL; }
    wuos_tb_count = 0;
    if (!tb) return;
    size_t n = tb->n;
    if (n == 0) return;
    wuos_tb_buttons = calloc(n, sizeof *wuos_tb_buttons);
    if (!wuos_tb_buttons) return;
    for (size_t i = 0; i < n; i++){
        wuos_tb_buttons[i].label = tb->items[i].label;  /* points into hive */
        wuos_tb_buttons[i].cmd   = tb->items[i].cmd;
    }
    wuos_tb_count = n;
}

void wuos_tb_shutdown(void){
    free(wuos_tb_buttons); wuos_tb_buttons = NULL; wuos_tb_count = 0;
}

int wuos_tb_label_count(void){
    int n=0;
    for (size_t i=0;i<wuos_tb_count;i++) if (wuos_tb_buttons[i].label) n++;
    return n;
}

/* Must stay in sync with the shell's render loop (main.c toolbar block). */
#define TB_SEP_GAP  10
#define TB_CHAR_W   7
#define TB_PAD      14
#define TB_GAP      1

int wuos_tb_at(int x){
    int cur=0;
    for (size_t i=0;i<wuos_tb_count;i++){
        if (!wuos_tb_buttons[i].label){ cur += TB_SEP_GAP; continue; }
        int bw = (int)strlen(wuos_tb_buttons[i].label)*TB_CHAR_W + TB_PAD;
        if (x>=cur && x<cur+bw) return (int)i;
        cur += bw + TB_GAP;
    }
    return -1;
}

int wuos_tb_width(void){
    int cur=0;
    for (size_t i=0;i<wuos_tb_count;i++){
        if (!wuos_tb_buttons[i].label){ cur += TB_SEP_GAP; continue; }
        cur += (int)strlen(wuos_tb_buttons[i].label)*TB_CHAR_W + TB_PAD + TB_GAP;
    }
    return cur;
}

int wuos_tb_cmd_to_key(int cmd){
    switch (cmd){
    case 11: return WUOS_KEY_STYLE_H1;
    case 12: return WUOS_KEY_STYLE_H2;
    case 13: return WUOS_KEY_STYLE_H3;
    case 14: return WUOS_KEY_STYLE_BODY;
    case 15: return WUOS_KEY_STYLE_QUOTE;
    case 16: return WUOS_KEY_STYLE_CODE;
    case 17: return WUOS_KEY_INSERT_LINK;
    case 18: return WUOS_KEY_INSERT_LIST;
    case 19: return WUOS_KEY_INSERT_TABLE;
    case 20: return WUOS_KEY_INSERT_IMAGE;
    default: return 0;
    }
}
