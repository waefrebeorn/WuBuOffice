/* sync.c -- local-first CRDT store + shared lock. See sync.h. */
#include "sync.h"
#include "crdt.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct Sync {
    char dir[PATH_MAX];
};

static void path_for(Sync *s, const char *key, const char *suffix, char *out){
    /* sanitize key into a filename */
    char safe[256]; int k=0;
    for (const char *p=key; *p && k<255; p++){
        char c = *p;
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-')
            safe[k++]=c; else safe[k++]='_';
    }
    safe[k]=0;
    snprintf(out, PATH_MAX, "%s/%s%s", s->dir, safe, suffix? suffix : "");
}

Sync *sync_open(const char *dir){
    if (!dir) return NULL;
    struct stat st;
    if (stat(dir,&st)!=0){ if (mkdir(dir,0755)!=0) return NULL; }
    Sync *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    strncpy(s->dir, dir, PATH_MAX-1);
    return s;
}

void sync_close(Sync *s){ free(s); }

int sync_put(Sync *s, const char *key, const char *blob, size_t len, const char *site){
    if (!s || !key || !blob) return 0;
    char path[PATH_MAX]; path_for(s, key, ".crdt", path);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    /* header: site + len, then blob */
    fprintf(f, "%s %zu\n", site? site : "?", (size_t)len);
    size_t w = fwrite(blob, 1, len, f);
    fclose(f);
    return w == len ? 1 : 0;
}

int sync_get(Sync *s, const char *key, char **out_blob, size_t *out_len){
    if (!s || !key || !out_blob) return 0;
    char path[PATH_MAX]; path_for(s, key, ".crdt", path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char hdr[256]; if (!fgets(hdr, sizeof hdr, f)){ fclose(f); return 0; }
    size_t len = 0; if (sscanf(hdr, "%*s %zu", &len) != 1){ fclose(f); return 0; }
    char *buf = malloc(len ? len : 1);
    if (!buf){ fclose(f); return 0; }
    size_t r = fread(buf, 1, len, f);
    fclose(f);
    if (r != len){ free(buf); return 0; }
    *out_blob = buf; *out_len = len;
    return 1;
}

int sync_merge(Sync *s, const char *key, const char *peer_blob, size_t len, const char *site){
    if (!s || !key || !peer_blob) return -1;
    char *mine = NULL; size_t mylen = 0;
    if (!sync_get(s, key, &mine, &mylen)){  /* no local yet -> just store peer */
        return sync_put(s, key, peer_blob, len, site) ? 0 : -1;
    }
    Crdt *a = crdt_create(site? site : "local");
    Crdt *b = crdt_create("peer");
    if (!crdt_deserialize(a, mine, mylen) || !crdt_deserialize(b, peer_blob, len)){
        crdt_destroy(a); crdt_destroy(b); free(mine); return -1;
    }
    int added = crdt_merge(a, b);
    size_t slen; char *ser = crdt_serialize(a, &slen);
    int ok = ser ? sync_put(s, key, ser, slen, site) : 0;
    free(ser); crdt_destroy(a); crdt_destroy(b); free(mine);
    return ok ? added : -1;
}

int sync_lock(Sync *s, const char *key, int pid, const char *site){
    if (!s || !key) return 0;
    char path[PATH_MAX]; path_for(s, key, ".lock", path);
    FILE *f = fopen(path, "rb");
    if (f){
        int holder=0; char hs[64]; hs[0]=0;
        if (fscanf(f, "%d %63s", &holder, hs)==2){
            /* is the holder still alive? */
            if (holder == pid || (holder>0 && kill(holder, 0)==0)){
                fclose(f); return 0;  /* held by a live process */
            }
        }
        fclose(f);  /* stale lock -> reclaim */
    }
    f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "%d %s\n", pid, site? site : "?");
    fclose(f);
    return 1;
}

int sync_unlock(Sync *s, const char *key, int pid){
    if (!s || !key) return 0;
    char path[PATH_MAX]; path_for(s, key, ".lock", path);
    /* only remove if it names this pid (don't stomp another holder) */
    FILE *f = fopen(path, "rb");
    if (f){
        int holder=0; char hs[64]; hs[0]=0;
        if (fscanf(f, "%d %63s", &holder, hs)==2 && holder != pid){ fclose(f); return 1; }
        fclose(f);
    }
    unlink(path);
    return 1;
}
