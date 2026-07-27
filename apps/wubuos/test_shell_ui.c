/* test_shell_ui.c -- headless checks for the shell's pure-logic UI modules:
 * toast queue (UI-33) and command palette (UI-29). */
#include "toast.h"
#include "palette.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

static void test_toast(void){
    Toasts *t = toast_create();
    CK(toast_text(t) == NULL, "empty queue -> no text");
    toast_push(t, "saved", 2);
    toast_push(t, "exported PDF", 3);
    CK(toast_count(t) == 2, "two queued");
    CK(strcmp(toast_text(t), "saved") == 0, "head visible first");
    toast_tick(t);                       /* ttl 2 -> 1 */
    CK(strcmp(toast_text(t), "saved") == 0, "still visible at ttl 1");
    toast_tick(t);                       /* expires */
    CK(strcmp(toast_text(t), "exported PDF") == 0, "next message shown");
    toast_tick(t); toast_tick(t); toast_tick(t);
    CK(toast_text(t) == NULL, "queue drains");
    /* overflow safety */
    for (int i=0;i<100;i++) toast_push(t, "x", 1);
    CK(toast_count(t) <= 32, "bounded queue");
    toast_destroy(t);
}

static void test_palette(void){
    Palette *p = palette_create();
    palette_add(p, "Open File",        1);
    palette_add(p, "New Document",     2);
    palette_add(p, "Toggle Theme",     3);
    palette_add(p, "Export PDF",       4);
    palette_add(p, "Export EPUB",      5);
    palette_add(p, "Zoom In",          6);
    CK(!palette_is_open(p), "starts closed");
    palette_open(p);
    CK(palette_is_open(p), "opens");
    CK(palette_result_count(p) == 6, "empty query lists all");
    /* filter: "exp" should rank the two Export commands first */
    palette_input(p, 'e'); palette_input(p, 'x'); palette_input(p, 'p');
    CK(palette_result_count(p) == 2, "exp -> two results");
    CK(strncmp(palette_result_label(p, 0), "Export", 6) == 0, "prefix ranked first");
    /* select second result */
    palette_next(p);
    int id = palette_confirm(p);
    CK(id == 5, "confirm returns selected id (EPUB)");
    CK(!palette_is_open(p), "confirm closes");
    /* subsequence: "nwd" matches New Document only (N..w..D..) */
    palette_open(p);
    palette_input(p,'n'); palette_input(p,'w'); palette_input(p,'d');
    CK(palette_result_count(p) == 1, "scattered subsequence matches");
    CK(palette_result_id(p, 0) == 2, "matched New Document");
    /* backspace widens the filter again */
    palette_backspace(p); palette_backspace(p); palette_backspace(p);
    CK(palette_result_count(p) == 6, "backspace restores all");
    /* no match -> confirm returns -1 */
    palette_input(p,'z'); palette_input(p,'z'); palette_input(p,'z');
    CK(palette_result_count(p) == 0, "zzz matches nothing");
    CK(palette_confirm(p) == -1, "confirm with no results -> -1");
    palette_destroy(p);
}

int main(void){
    test_toast();
    test_palette();
    if (fails == 0) printf("SHELL UI TESTS PASSED\n");
    else printf("SHELL UI TESTS FAILED (%d)\n", fails);
    return fails ? 1 : 0;
}
