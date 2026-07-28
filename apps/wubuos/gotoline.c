/* gotoline.c -- opaque "Go to line" mini-prompt (see gotoline.h).
 * Ported out of view_editor.c; the editor now owns only a GotoLine* handle.
 */
#include "gotoline.h"
#include <stdlib.h>
#include <string.h>

struct GotoLine {
    int   active;
    char  buf[32];
};

GotoLine *gotoline_create(void){
    return calloc(1, sizeof(GotoLine));
}
void gotoline_destroy(GotoLine *g){ free(g); }

void gotoline_open(GotoLine *g){
    if (!g) return;
    g->active = 1;
    g->buf[0] = '\0';
}
void gotoline_close(GotoLine *g){ if (g) g->active = 0; }
int  gotoline_active(const GotoLine *g){ return g ? g->active : 0; }

const char *gotoline_buf(const GotoLine *g){ return g ? g->buf : ""; }

int gotoline_key(GotoLine *g, int key){
    if (!g || !g->active) return 0;
    if (key == 27 /*Esc*/){ gotoline_close(g); return 2; }
    if (key == '\n' || key == '\r' || key == 13){ return 1; }  /* commit */
    if (key == 8 || key == 127){                                /* backspace */
        size_t l = strlen(g->buf);
        if (l) g->buf[l-1] = '\0';
        return 0;
    }
    if (key >= '0' && key <= '9'){
        size_t l = strlen(g->buf);
        if (l < sizeof(g->buf) - 1){ g->buf[l] = (char)key; g->buf[l+1] = '\0'; }
        return 0;
    }
    if (key == 7) return 0;  /* Ctrl+G re-trigger: ignore */
    gotoline_close(g);        /* any other key dismisses */
    return 2;
}

int gotoline_commit(const GotoLine *g){
    if (!g) return 0;
    int ln = (g->buf[0]) ? atoi(g->buf) : 0;
    ((GotoLine*)g)->active = 0;   /* close on commit */
    return ln;                    /* 1-based line, 0 if empty/invalid */
}
