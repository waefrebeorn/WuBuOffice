/* test_shortcut_conformance.c -- U1: keyboard shortcut conformance gate.
 * Research (Microsoft keyboard-interactions, Apple HIG): standard shortcuts
 * are ACCESSIBILITY infrastructure. Deviations from the convention table are
 * bugs. This test pins the shell's keymap to the convention.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

int main(void){
    /* The shell keymap lives in wuos_shell_events.c as a sequence of
     * "k==SDLK_x && (mod & KMOD_CTRL)) code=WUOS_KEY_Y" lines.
     * Parse the source and verify each binding against the convention. */
    FILE *f = fopen(WUBUOS_SRC "/wuos_shell_events.c", "r");
    if (!f){ fprintf(stderr,"cannot open keymap source\n"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)sz + 1);
    size_t rd = fread(src, 1, (size_t)sz, f);
    src[rd] = 0;
    fclose(f);

    /* Check each convention binding by scanning a context window around
     * every occurrence of the key+Ctrl pattern (bindings may wrap lines). */
    int found = 0;
    struct { const char *key; const char *want; const char *label; } conv[] = {
        {"SDLK_s",  "WUOS_KEY_SAVE",       "Ctrl+S -> Save"},
        {"SDLK_z",  "WUOS_KEY_UNDO",       "Ctrl+Z -> Undo"},
        {"SDLK_c",  "WUOS_KEY_COPY",       "Ctrl+C -> Copy"},
        {"SDLK_v",  "WUOS_KEY_PASTE",      "Ctrl+V -> Paste"},
        {"SDLK_x",  "WUOS_KEY_CUT",        "Ctrl+X -> Cut"},
        {"SDLK_a",  "WUOS_KEY_SELECT_ALL", "Ctrl+A -> Select All"},
        {"SDLK_y",  "WUOS_KEY_REDO",       "Ctrl+Y -> Redo"},
        {"SDLK_o",  "dialog_open",        "Ctrl+O -> Open dialog"},
        {NULL,NULL,NULL}
    };
    for (int i = 0; conv[i].key; i++){
        const char *p = src;
        int checked = 0, ok_all = 1;
        while ((p = strstr(p, conv[i].key)) != NULL){
            /* context: this occurrence through the next 200 chars */
            char win[256]; size_t n = strlen(p); if (n > 200) n = 200;
            memcpy(win, p, n); win[n] = 0;
            if (strstr(win, "KMOD_CTRL")){
                checked++;
                /* strip KMOD_SHIFT/KMOD_ALT variants: those are INTENTIONAL
                 * different commands (Ctrl+Shift+S = Save As), only plain
                 * Ctrl+<key> must match the convention. */
                const char *mod = strstr(win, "KMOD_CTRL");
                const char *lineend = strchr(mod, '\n');
                char plain[256];
                size_t plen = lineend ? (size_t)(lineend - mod) : strlen(mod);
                if (plen > 255) plen = 255;
                memcpy(plain, mod, plen); plain[plen] = 0;
                if (strstr(plain, "KMOD_SHIFT") || strstr(plain, "KMOD_ALT")){
                    p += strlen(conv[i].key); continue;   /* variant binding */
                }
                const char *codep = strstr(plain, "code=");
                /* two valid binding forms: direct code assignment, or a
                 * dialog action block (Ctrl+O / Ctrl+Shift+S open dialogs) */
                int ok_direct = codep && strstr(codep, conv[i].want);
                int ok_dialog = strstr(win, "dialog_open") != NULL
                             || strstr(win, "g_dlg_action") != NULL;
                if (!ok_direct && !(ok_dialog && strstr(conv[i].want, "dialog"))) ok_all = 0;
            }
            p += strlen(conv[i].key);
        }
        ck(checked > 0 && ok_all, conv[i].label);
        if (checked > 0) found++;
    }
    free(src);
    ck(found >= 6, "convention table matched >= 6 bindings in keymap");

    fprintf(stderr, bad ? "SHORTCUT_CONFORMANCE FAIL\n"
                        : "SHORTCUT_CONFORMANCE PASS\n");
    return bad ? 1 : 0;
}
