/* load_async.c — background document loader (ws07#1334).
 * A single worker thread drains the queue, running each wubumodel_load_fn
 * off the caller's thread. Results are collected and the done callbacks
 * fire on load_async_join()'s thread (the UI thread), so they may
 * touch UI state without extra locking. Native pthreads, no Electron. */

#include "load_async.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct la_job {
    char *path;
    wubumodel_load_fn fn;
    wubumodel_load_done done;
    void *user;
    wubumodel_doc *doc;     /* result, filled by worker */
    int ok;                  /* 1 if doc != NULL */
    struct la_job *next;
} la_job;

struct load_async {
    pthread_mutex_t lock;
    pthread_cond_t  cond;     /* signaled when a job is queued */
    pthread_t worker;
    int stop;                 /* set on destroy */
    int alive;                /* worker running */
    la_job *head, *tail;     /* queue */
    la_job *done_head;         /* completed, awaiting join-callback */
};

/* Worker: drain the queue, run each fn, move to done list. */
static void *la_worker(void *arg) {
    load_async *la = arg;
    for (;;) {
        pthread_mutex_lock(&la->lock);
        while (!la->head && !la->stop) {
            pthread_cond_wait(&la->cond, &la->lock);
        }
        if (la->stop && !la->head) {
            pthread_mutex_unlock(&la->lock);
            break;
        }
        la_job *j = la->head;
        if (j) {
            la->head = j->next;
            if (la->tail == j) la->tail = NULL;
        }
        pthread_mutex_unlock(&la->lock);
        if (!j) continue;

        /* Run the (potentially heavy) load OFF the caller thread. */
        j->ok = (j->fn && j->fn(j->path, &j->doc) == 0 && j->doc);
        if (!j->ok) j->doc = NULL;

        /* Move to the completed list for join-time callbacks. */
        pthread_mutex_lock(&la->lock);
        j->next = la->done_head;
        la->done_head = j;
        pthread_mutex_unlock(&la->lock);
    }
    return NULL;
}

load_async *load_async_create(void) {
    load_async *la = calloc(1, sizeof(*la));
    if (!la) return NULL;
    pthread_mutex_init(&la->lock, NULL);
    pthread_cond_init(&la->cond, NULL);
    la->stop = 0;
    if (pthread_create(&la->worker, NULL, la_worker, la) != 0) {
        pthread_mutex_destroy(&la->lock);
        pthread_cond_destroy(&la->cond);
        free(la);
        return NULL;
    }
    la->alive = 1;
    return la;
}

void load_async_destroy(load_async *la) {
    if (!la) return;
    pthread_mutex_lock(&la->lock);
    la->stop = 1;
    pthread_cond_signal(&la->cond);
    pthread_mutex_unlock(&la->lock);
    if (la->alive) pthread_join(la->worker, NULL);

    /* Fire any undelivered callbacks, then free everything. */
    la_job *j = la->done_head;
    while (j) {
        la_job *nx = j->next;
        if (j->done) j->done(j->path, j->doc, j->user);
        if (j->doc) wubumodel_doc_destroy(j->doc);
        free(j->path);
        free(j);
        j = nx;
    }
    /* Also free any still-queued (unstarted) jobs. */
    j = la->head;
    while (j) {
        la_job *nx = j->next;
        free(j->path);
        free(j);
        j = nx;
    }
    pthread_mutex_destroy(&la->lock);
    pthread_cond_destroy(&la->cond);
    free(la);
}

int load_async_queue(load_async *la, const char *path,
                     wubumodel_load_fn fn, wubumodel_load_done done,
                     void *user) {
    if (!la || !path || !fn) return -1;
    la_job *j = calloc(1, sizeof(*j));
    if (!j) return -1;
    j->path = strdup(path);
    if (!j->path) { free(j); return -1; }
    j->fn = fn;
    j->done = done;
    j->user = user;

    pthread_mutex_lock(&la->lock);
    j->next = NULL;
    if (la->tail) la->tail->next = j; else la->head = j;
    la->tail = j;
    pthread_cond_signal(&la->cond);
    pthread_mutex_unlock(&la->lock);
    return 0;
}

void load_async_join(load_async *la) {
    if (!la) return;
    for (;;) {
        pthread_mutex_lock(&la->lock);
        /* Pull one completed job off the done list. */
        la_job *j = la->done_head;
        if (j) la->done_head = j->next;
        pthread_mutex_unlock(&la->lock);
        if (!j) break;

        /* Fire on THIS (joining / UI) thread. */
        if (j->done) j->done(j->path, j->doc, j->user);
        if (j->doc) wubumodel_doc_destroy(j->doc);
        free(j->path);
        free(j);
    }
}
