/* macro.c -- opaque macro record/playback engine (see macro.h).
 * Ported out of view_editor.c; the editor now owns only a Macro* handle and
 * a thin dispatch that forwards captured keys + replays via on_key.
 */
#include "macro.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MACRO_CAP 8192   /* max recorded op bytes (matches prior buffer) */

struct Macro {
    int   rec;            /* recording flag */
    unsigned char ops[MACRO_CAP];
    int   len;            /* bytes used in ops[] */
    int   n;              /* recorded op count */
    char  name[64];       /* label for persistence */
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

void macro_set_name(Macro *m, const char *name){
    if (!m) return;
    if (name) snprintf(m->name, sizeof m->name, "%s", name);
    else m->name[0] = '\0';
}
const char *macro_name(const Macro *m){ return m ? m->name : ""; }

/* persistence: file format is "<name>\t<hex-bytes...>\n" */
int macro_save(const char *path){
    Macro *m = g_shared;
    if (!m || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\t", m->name[0] ? m->name : "macro");
    for (int i = 0; i < m->len; i++) fprintf(f, "%02X", m->ops[i]);
    fprintf(f, "\n");
    fclose(f);
    return 0;
}

int macro_load(const char *path){
    Macro *m = g_shared;
    if (!m || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char nm[64]; int c = fscanf(f, "%63[^\t]\t", nm);
    if (c != 1){ fclose(f); return -1; }
    snprintf(m->name, sizeof m->name, "%s", nm);
    m->len = 0; m->n = 0; m->rec = 0;
    int hi;
    while (fscanf(f, "%2x", &hi) == 1){
        if (m->len >= MACRO_CAP) break;
        m->ops[m->len++] = (unsigned char)hi;
    }
    for (int i = 0; i < m->len; ){ m->n++; i += (m->ops[i] == MACRO_OP_CHAR) ? 2 : 1; }
    fclose(f);
    return 0;
}
