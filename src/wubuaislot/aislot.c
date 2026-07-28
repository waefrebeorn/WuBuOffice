/* aislot.c -- offline AI assist hook. See aislot.h.
 *
 * Built-in fallback is a deterministic, offline heuristic:
 *  - "summarize": first sentence of each paragraph (up to 3).
 *  - "complete":  echoes the prompt's last line (a safe no-model default).
 * A real provider (local GGUF runner, RPC bridge) replaces it via
 * aislot_set_provider without touching callers. */
#include "aislot.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct AiSlot {
    AiProviderFn fn;
    void *user;
};

AiSlot *aislot_create(void){ return calloc(1, sizeof(AiSlot)); }
void aislot_destroy(AiSlot *s){ free(s); }

void aislot_set_provider(AiSlot *s, AiProviderFn fn, void *user){
    if (!s) return;
    s->fn = fn; s->user = user;
}
int aislot_has_custom_provider(const AiSlot *s){ return s && s->fn ? 1 : 0; }

static int fallback(const char *task, const char *prompt, char *out, size_t cap){
    if (!task || !prompt || !out || cap==0) return 1;
    out[0]=0;
    if (strcmp(task, "summarize")==0){
        /* first sentence of up to 3 paragraphs */
        size_t o=0; int sents=0;
        const char *p = prompt;
        while (*p && sents<3){
            /* skip leading blank lines */
            while (*p=='\n' || *p==' ') p++;
            const char *start = p;
            while (*p && *p!='.' && *p!='\n') p++;
            size_t l = (size_t)(p-start);
            if (l>0){
                if (*p=='.') l++;
                if (o+l+2 >= cap) break;
                memcpy(out+o, start, l); o+=l;
                out[o++]='\n';
                sents++;
            }
            /* jump to next paragraph */
            while (*p && *p!='\n') p++;
        }
        out[o]=0;
        return 0;
    }
    if (strcmp(task, "complete")==0){
        const char *last = strrchr(prompt, '\n');
        last = last ? last+1 : prompt;
        snprintf(out, cap, "%s ...", last);
        return 0;
    }
    return 1;  /* unknown task */
}

char *aislot_run(AiSlot *s, const char *task, const char *prompt){
    if (!s || !task || !prompt) return NULL;
    char buf[4096];
    int rc = s->fn ? s->fn(s->user, task, prompt, buf, sizeof buf)
                   : fallback(task, prompt, buf, sizeof buf);
    if (rc != 0) return NULL;
    return strdup(buf);
}
