/* autosave.c -- dependency-free C11 autosave + crash-recovery (see autosave.h) */
#include "autosave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

/* ---------- path helpers ---------- */
static char *cat3(const char *base, const char *mid, const char *ext){
    size_t n = strlen(base) + strlen(mid) + strlen(ext) + 1;
    char *s = malloc(n);
    if (!s) return NULL;
    snprintf(s, n, "%s%s%s", base, mid, ext);
    return s;
}
/* derive "<doc>.asd" */
static char *asd_path(const char *doc){ return cat3(doc, "", ".asd"); }
/* derive "<doc>.lock" */
static char *lock_path(const char *doc){ return cat3(doc, "", ".lock"); }

/* ---------- lock (PID owner file) ---------- */
static int lock_acquire(const char *lk){
    int fd = open(lk, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    char buf[32];
    int n = snprintf(buf, sizeof buf, "%d", (int)getpid());
    if (write(fd, buf, (size_t)n) != (ssize_t)n){ close(fd); return -1; }
    fsync(fd);
    close(fd);
    return 0;
}
static int pid_alive(int pid){
    if (pid <= 0) return 0;
    /* kill(pid,0): success => process exists; ESRCH => dead; EPERM => exists */
    return kill(pid, 0) == 0 || errno == EPERM;
}
/* 1 if lock exists AND its PID is a live process (a live owner) */
static int lock_is_live(const char *lk){
    FILE *f = fopen(lk, "r");
    if (!f) return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1){ fclose(f); return 0; }
    fclose(f);
    return pid_alive(pid);
}
static void lock_release(const char *lk){ unlink(lk); }

/* ---------- atomic write ---------- */
static int atomic_write(const char *final_path, const char *data, size_t len){
    char *tmp = malloc(strlen(final_path) + 16);
    if (!tmp) return -1;
    snprintf(tmp, strlen(final_path) + 16, "%s.tmp%ld", final_path, (long)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0){ free(tmp); return -1; }
    size_t off = 0;
    while (off < len){
        ssize_t w = write(fd, data + off, len - off);
        if (w < 0){ close(fd); unlink(tmp); free(tmp); return -1; }
        off += (size_t)w;
    }
    fsync(fd);
    close(fd);
    if (rename(tmp, final_path) != 0){ unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* ---------- session ---------- */
struct Autosave {
    char *doc;
    char *asd;
    char *lock;
    int   interval_ms;
    int   dirty;
    long  last_ms;   /* last snapshot time, ms */
};

static long now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

Autosave *wubuautosave_create(const char *doc_path, int interval_ms){
    Autosave *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->doc = strdup(doc_path);
    a->asd = asd_path(doc_path);
    a->lock = lock_path(doc_path);
    if (!a->doc || !a->asd || !a->lock){ free(a->doc); free(a->asd); free(a->lock); free(a); return NULL; }
    a->interval_ms = interval_ms > 0 ? interval_ms : 0;
    a->last_ms = 0;
    if (lock_acquire(a->lock) != 0){
        /* best effort: continue without lock (still autosaves) */
    }
    return a;
}

void wubuautosave_destroy(Autosave *a){
    if (!a) return;
    free(a->doc); free(a->asd); free(a->lock);
    free(a);
}

void wubuautosave_mark_dirty(Autosave *a){ if (a) a->dirty = 1; }
void wubuautosave_clear_dirty(Autosave *a){ if (a) a->dirty = 0; }
void wubuautosave_set_interval(Autosave *a, int ms){ if (a) a->interval_ms = ms > 0 ? ms : 0; }

/* serialize the doc to a docx byte buffer via wubumodel, then atomic-write */
static int snapshot(Autosave *a, const wubumodel_doc *doc){
    /* wubumodel_write_docx emits a file; to keep it atomic we write to a temp
     * docx path then rename. Reuse asd path with .docx.tmp suffix. */
    char *tmpdocx = malloc(strlen(a->asd) + 32);
    if (!tmpdocx) return -1;
    snprintf(tmpdocx, strlen(a->asd) + 32, "%s.docx.tmp%ld", a->asd, (long)getpid());
    if (wubumodel_write_docx(doc, tmpdocx) != 0){ free(tmpdocx); return -1; }
    /* read it back and atomic-write to the .asd target */
    FILE *f = fopen(tmpdocx, "rb");
    if (!f){ free(tmpdocx); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf){ fclose(f); free(tmpdocx); return -1; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    int rc = atomic_write(a->asd, buf, got);
    free(buf);
    unlink(tmpdocx);
    free(tmpdocx);
    return rc;
}

int wubuautosave_tick(Autosave *a, const wubumodel_doc *doc){
    if (!a) return -1;
    if (!a->dirty) return 0;
    if (a->interval_ms <= 0) return 0;   /* interval 0 => only explicit flush() writes */
    long t = now_ms();
    if ((t - a->last_ms) < a->interval_ms) return 0;
    if (snapshot(a, doc) != 0) return -1;
    a->last_ms = t;
    a->dirty = 0;
    return 1;
}

int wubuautosave_flush(Autosave *a, const wubumodel_doc *doc){
    if (!a) return -1;
    if (snapshot(a, doc) != 0) return -1;
    a->last_ms = now_ms();
    a->dirty = 0;
    return 0;
}

int wubuautosave_clear(Autosave *a){
    if (!a) return -1;
    if (a->asd) unlink(a->asd);
    if (a->lock) lock_release(a->lock);
    return 0;
}

/* ---------- recovery ---------- */
int wubuautosave_has_recovery(const char *doc_path){
    char *asd = asd_path(doc_path);
    char *lk = lock_path(doc_path);
    int has_asd = (access(asd, F_OK) == 0);
    int live = lock_is_live(lk);
    free(asd); free(lk);
    return has_asd && !live ? 1 : 0;
}

int wubuautosave_recover(const char *doc_path, wubumodel_doc **out){
    *out = NULL;
    char *asd = asd_path(doc_path);
    if (access(asd, F_OK) != 0){ free(asd); return 0; }
    int r = wubumodel_load_docx(asd, out);
    free(asd);
    if (r != 0) return -1;
    return 1;
}

int wubuautosave_discard_recovery(const char *doc_path){
    char *asd = asd_path(doc_path);
    char *lk = lock_path(doc_path);
    unlink(asd);
    unlink(lk);
    free(asd); free(lk);
    return 0;
}
