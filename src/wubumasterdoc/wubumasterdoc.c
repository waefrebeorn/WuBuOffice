#include "wubumasterdoc.h"
#include <stdlib.h>
#include <string.h>

struct wubumasterdoc {
    char **paths;
    size_t n, cap;
};

wubumasterdoc *wubumasterdoc_create(void) {
    return (wubumasterdoc *)calloc(1, sizeof(wubumasterdoc));
}

void wubumasterdoc_destroy(wubumasterdoc *m) {
    if (!m) return;
    for (size_t i = 0; i < m->n; i++) free(m->paths[i]);
    free(m->paths);
    free(m);
}

static int grow(wubumasterdoc *m, size_t at_least) {
    if (at_least > m->cap) {
        size_t nc = m->cap ? m->cap : 4;
        while (nc < at_least) nc *= 2;
        char **np = (char **)realloc(m->paths, nc * sizeof(char *));
        if (!np) return -1;
        m->paths = np; m->cap = nc;
    }
    return 0;
}

int wubumasterdoc_add(wubumasterdoc *m, const char *path) {
    return wubumasterdoc_insert(m, m ? m->n : 0, path);
}

int wubumasterdoc_insert(wubumasterdoc *m, size_t at, const char *path) {
    if (!m || !path || at > m->n) return -1;
    if (grow(m, m->n + 1) != 0) return -1;
    char *cp = strdup(path);
    if (!cp) return -1;
    memmove(&m->paths[at + 1], &m->paths[at], (m->n - at) * sizeof(char *));
    m->paths[at] = cp;
    m->n++;
    return 0;
}

int wubumasterdoc_remove(wubumasterdoc *m, size_t i) {
    if (!m || i >= m->n) return 0;
    free(m->paths[i]);
    memmove(&m->paths[i], &m->paths[i + 1], (m->n - i - 1) * sizeof(char *));
    m->n--;
    return 1;
}

size_t wubumasterdoc_count(const wubumasterdoc *m) { return m ? m->n : 0; }
const char *wubumasterdoc_get(const wubumasterdoc *m, size_t i) {
    return (m && i < m->n) ? m->paths[i] : NULL;
}
