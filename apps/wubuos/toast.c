/* toast.c -- UI-33 toast queue. FIFO of copied strings with per-message TTL. */
#include "toast.h"
#include <stdlib.h>
#include <string.h>

#define TOAST_MAX 32

struct Toasts {
    char *msg[TOAST_MAX];
    int   ttl[TOAST_MAX];
    int   head, count;
};

Toasts *toast_create(void){ return calloc(1, sizeof(Toasts)); }

void toast_destroy(Toasts *t){
    if (!t) return;
    for (int i=0;i<t->count;i++) free(t->msg[(t->head+i)%TOAST_MAX]);
    free(t);
}

void toast_push(Toasts *t, const char *msg, int ttl_ticks){
    if (!t || !msg || t->count >= TOAST_MAX) return;
    int slot = (t->head + t->count) % TOAST_MAX;
    t->msg[slot] = strdup(msg);
    t->ttl[slot] = ttl_ticks > 0 ? ttl_ticks : 1;
    t->count++;
}

void toast_tick(Toasts *t){
    if (!t || t->count == 0) return;
    if (--t->ttl[t->head] <= 0){
        free(t->msg[t->head]);
        t->msg[t->head] = NULL;
        t->head = (t->head + 1) % TOAST_MAX;
        t->count--;
    }
}

const char *toast_text(const Toasts *t){
    if (!t || t->count == 0) return NULL;
    return t->msg[t->head];
}

int toast_count(const Toasts *t){ return t ? t->count : 0; }
