#include "wuos_toolbar.h"
#include "wuos.h"   /* WUOS_KEY_STYLE_* / WUOS_KEY_INSERT_* sentinels */
#include <string.h>

/* Toolbar buttons. cmd >=1000 = menu/palette command (run via run_menu_cmd);
 * cmd 11..20 = formatting/insert forwarded to the active view's on_key. */
const WuosTbBtn wuos_tb_buttons[] = {
  { "New", 1003 }, { "Open", 1000 }, { "Save", 1001 }, { "Save As", 1002 },
  { NULL, 0 },
  { "Undo", 1005 }, { "Redo", 1006 },
  { NULL, 0 },
  { "Find", 1007 }, { "Replace", 1008 },
  { NULL, 0 },
  { "H1", 11 }, { "H2", 12 }, { "H3", 13 }, { "Body", 14 }, { "Quote", 15 }, { "Code", 16 },
  { NULL, 0 },
  { "Link", 17 }, { "List", 18 }, { "Table", 19 }, { "Image", 20 },
  { NULL, 0 },
  { "PDF", 1030 }, { "EPUB", 1004 },
};
const size_t wuos_tb_count = sizeof wuos_tb_buttons / sizeof wuos_tb_buttons[0];

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
