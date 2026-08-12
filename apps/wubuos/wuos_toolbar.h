/* wuos_toolbar.h -- quick-access + formatting toolbar model for the WuBuOffice
 * shell. Pure logic (button table + hit-testing + command mapping); the shell
 * renders it and dispatches the returned command to the active view. Clean C11.
 * UI gap closed: reference office apps (LibreOffice/OnlyOffice/MS Office) and
 * Notepad++ all expose a clickable button strip; the shell reached these
 * commands only via keyboard / menu / palette before this existed. */
#ifndef WUOS_TOOLBAR_H
#define WUOS_TOOLBAR_H
#include <stddef.h>

/* One toolbar button. label==NULL marks a vertical separator (hit-testing
 * skips it). cmd is a command id: >=1000 is a menu/palette command id;
 * 11..20 is a formatting/insert key (see tb_cmd_to_key). */
typedef struct { const char *label; int cmd; } WuosTbBtn;

/* The button table (terminated naturally by sizeof / no sentinel needed). */
extern const WuosTbBtn wuos_tb_buttons[];
extern const size_t    wuos_tb_count;

/* Number of label (clickable) buttons. */
int wuos_tb_label_count(void);

/* Index of the button under x (over the toolbar row, starting at offset 0),
 * or -1. Mirror of the render layout: separators advance 10px, a button is
 * 7*strlen(label)+14 wide with a 1px gap after. */
int wuos_tb_at(int x);

/* Total width the toolbar spans (so the shell can assert it fits the window). */
int wuos_tb_width(void);

/* Map a formatting/insert toolbar command (11..20) to a WUOS_KEY sentinel.
 * Returns 0 for menu-space commands (call run_menu_cmd instead). */
int wuos_tb_cmd_to_key(int cmd);

#endif
