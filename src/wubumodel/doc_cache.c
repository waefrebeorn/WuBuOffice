/* doc_cache.c — opaque, thread-safe parsed-document cache (ws07#1342).
 * Keyed by (path, file mtime, file size) so a changed file is never
 * served stale. The cache owns any doc it stores. */

#include "doc_cache.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#ifndef WUBUMODEL_INTERNAL
/* Keep the cache dependency-free of the internal model layout: we only
 * ever hold an opaque wubumodel_doc* and call its public destroy. */
#endif

#define DC_BUCKETS 1024

typedef struct dc_entry {
    char *path;
    long mtime_sec;
    long long size;
    wubumodel_doc *doc;
    struct dc_entry *next;
} dc_entry;

struct doc_cache {
    pthread_mutex_t lock;
    dc_entry *buckets[DC_BUCKETS];
};

/* FNV-1a over the path -> bucket. */
static size_t dc_hash(const char *s) {
    unsigned long h = 1469598103934665603UL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211UL;
    }
    return h % DC_BUCKETS;
}

/* Return mtime+size for `path` (0,0 if stat fails). */
static void dc_stat(const char *path, long *mtime, long long *size) {
    struct stat st;
    if (stat(path, &st) == 0) {
        *mtime = (long)st.st_mtime;
        *size  = (long long)st.st_size;
    } else {
        *mtime = 0;
        *size  = 0;
    }
}

doc_cache *doc_cache_create(void) {
    doc_cache *c = calloc(1, sizeof(*c));
    if (!c) return NULL;
    pthread_mutex_init(&c->lock, NULL);
    return c;
}

void doc_cache_destroy(doc_cache *c) {
    if (!c) return;
    pthread_mutex_lock(&c->lock);
    for (int i = 0; i < DC_BUCKETS; i++) {
        dc_entry *e = c->buckets[i];
        while (e) {
            dc_entry *nx = e->next;
            wubumodel_doc_destroy(e->doc);
            free(e->path);
            free(e);
            e = nx;
        }
        c->buckets[i] = NULL;
    }
    pthread_mutex_unlock(&c->lock);
    pthread_mutex_destroy(&c->lock);
    free(c);
}

wubumodel_doc *doc_cache_get(doc_cache *c, const char *path) {
    if (!c || !path) return NULL;
    long mt; long long sz;
    dc_stat(path, &mt, &sz);

    pthread_mutex_lock(&c->lock);
    size_t b = dc_hash(path);
    for (dc_entry *e = c->buckets[b]; e; e = e->next) {
        if (strcmp(e->path, path) == 0) {
            /* Entry exists: return it only if the file is unchanged. */
            if (e->mtime_sec == mt && e->size == sz) {
                wubumodel_doc *d = e->doc;
                pthread_mutex_unlock(&c->lock);
                return d;
            }
            /* Stale: drop it. */
            wubumodel_doc_destroy(e->doc);
            free(e->path);
            /* unlink */
            dc_entry **pp = &c->buckets[b];
            while (*pp && *pp != e) pp = &(*pp)->next;
            if (*pp) *pp = e->next;
            free(e);
            break;
        }
    }
    pthread_mutex_unlock(&c->lock);
    return NULL;
}

int doc_cache_put(doc_cache *c, const char *path,
                 long mtime_sec, long long size, wubumodel_doc *doc) {
    if (!c || !path || !doc) return -1;
    char *pc = strdup(path);
    if (!pc) return -1;

    pthread_mutex_lock(&c->lock);
    size_t b = dc_hash(path);
    /* Replace any existing entry for this path. */
    dc_entry **pp = &c->buckets[b];
    while (*pp) {
        dc_entry *e = *pp;
        if (strcmp(e->path, path) == 0) {
            wubumodel_doc_destroy(e->doc);
            free(e->path);
            e->path = pc;
            e->mtime_sec = mtime_sec;
            e->size = size;
            e->doc = doc;          /* cache takes ownership */
            pthread_mutex_unlock(&c->lock);
            return 0;
        }
        pp = &e->next;
    }
    dc_entry *e = calloc(1, sizeof(*e));
    if (!e) {
        free(pc);
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    e->path = pc;
    e->mtime_sec = mtime_sec;
    e->size = size;
    e->doc = doc;                 /* cache takes ownership */
    e->next = c->buckets[b];
    c->buckets[b] = e;
    pthread_mutex_unlock(&c->lock);
    return 0;
}

void doc_cache_invalidate(doc_cache *c, const char *path) {
    if (!c || !path) return;
    pthread_mutex_lock(&c->lock);
    size_t b = dc_hash(path);
    dc_entry **pp = &c->buckets[b];
    while (*pp) {
        dc_entry *e = *pp;
        if (strcmp(e->path, path) == 0) {
            *pp = e->next;
            wubumodel_doc_destroy(e->doc);
            free(e->path);
            free(e);
            break;
        }
        pp = &e->next;
    }
    pthread_mutex_unlock(&c->lock);
}
