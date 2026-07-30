/* dialog.c -- opaque modal text-input dialog (see dialog.h). */
#include "dialog.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DLG_TITLE 64
#define DLG_PROMPT 128
#define DLG_BUF 512

struct Dialog {
    char title[DLG_TITLE];
    char prompt[DLG_PROMPT];
    char buf[DLG_BUF];
    int  active;
    int  confirmed;
};

Dialog *dialog_create(void){
    Dialog *d = calloc(1, sizeof *d);
    return d;
}

void dialog_destroy(Dialog *d){ free(d); }

void dialog_open(Dialog *d, const char *title, const char *prompt, const char *def){
    if (!d) return;
    d->title[0] = '\0'; d->prompt[0] = '\0'; d->buf[0] = '\0';
    d->active = 1; d->confirmed = 0;
    if (title) snprintf(d->title, DLG_TITLE, "%s", title);
    if (prompt) snprintf(d->prompt, DLG_PROMPT, "%s", prompt);
    if (def) snprintf(d->buf, DLG_BUF, "%s", def);
}

int dialog_active(const Dialog *d){ return d ? d->active : 0; }

int dialog_confirmed(const Dialog *d){ return d ? d->confirmed : 0; }

const char *dialog_text(const Dialog *d){ return d ? d->buf : ""; }
const char *dialog_title(const Dialog *d){ return d ? d->title : ""; }
const char *dialog_prompt(const Dialog *d){ return d ? d->prompt : ""; }

int dialog_key(Dialog *d, int key, const char *ch){
    if (!d || !d->active) return 0;
    if (key == 27){            /* Esc -> cancel */
        d->active = 0; d->confirmed = 0;
        return 2;
    }
    if (key == 13 || key == 10){   /* Enter/Return -> confirm */
        d->active = 0; d->confirmed = 1;
        return 1;
    }
    if (key == 8){            /* Backspace */
        size_t L = strlen(d->buf);
        if (L) d->buf[L-1] = '\0';
        return 0;
    }
    if (key >= 32 && key < 127 && ch && *ch){   /* printable ASCII */
        size_t L = strlen(d->buf);
        if (L + 1 < DLG_BUF){
            d->buf[L]   = ch[0];
            d->buf[L+1] = '\0';
        }
        return 0;
    }
    return 0;   /* ignore other keys while open */
}
