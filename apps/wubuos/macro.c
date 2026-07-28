/* macro.c -- opaque macro record/playback engine (see macro.h).
 * Ported out of view_editor.c; the editor now owns only a Macro* handle and
 * a thin dispatch that forwards captured keys + replays via on_key.
 */
#include "macro.h"
#include <stdlib.h>
#include <string.h>

#define MACRO_CAP 8192   /* max recorded op bytes (matches prior buffer) */

struct Macro {
    int   rec;            /* recording flag */
    unsigned char ops[MACRO_CAP];
    int   len;            /* bytes used in ops[] */
    int   n;              /* recorded op count */
};

/* Notepad++ macros are process-global: one shared engine for the whole
 * session, so a macro recorded in one editor replays in another. macro_create
 * returns the same shared instance every call; macro_destroy is a no-op so
 * per-editor teardown never invalidates the shared buffer. */
static Macro *g_shared = NULL;

Macro *macro_create(void){
    if (!g_shared) g_shared = calloc(1, sizeof *g_shared);
    return g_shared;
}
void macro_destroy(Macro *m){ (void)m; /* singleton lives for process lifetime */ }

int macro_toggle_rec(Macro *m){
    if (!m) return 0;
    m->rec ^= 1;
    if (m->rec){ m->len = 0; m->n = 0; }
    return m->rec;
}
int macro_recording(const Macro *m){ return m ? m->rec : 0; }

void macro_record(Macro *m, int opcode, unsigned char ch){
    if (!m || !m->rec) return;
    if (m->len >= MACRO_CAP - 2) return;
    if (opcode == MACRO_OP_CHAR){
        m->ops[m->len++] = (unsigned char)MACRO_OP_CHAR;
        m->ops[m->len++] = ch;
    } else {
        m->ops[m->len++] = (unsigned char)opcode;
    }
    m->n++;
}

int macro_count(const Macro *m){ return m ? m->n : 0; }

void macro_play(const Macro *m, macro_op_cb cb, void *ctx){
    if (!m || !m->n || !cb) return;
    for (int i = 0; i < m->len; ){
        unsigned char op = m->ops[i++];
        if (op == MACRO_OP_CHAR){
            unsigned char ch = m->ops[i++];
            cb(MACRO_OP_CHAR, ch, ctx);
        } else if (op == MACRO_OP_RETURN){
            cb(MACRO_OP_RETURN, 0, ctx);
        } else if (op == MACRO_OP_BACKSPACE){
            cb(MACRO_OP_BACKSPACE, 0, ctx);
        }
    }
}
