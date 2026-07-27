/* plugin.c -- wubuos plugin loader implementation.
 * Portable dlopen-based loader. No Windows support is wired (WSL/Linux host);
 * the dlfcn calls degrade gracefully if a module fails to load. */
#include "plugin.h"

#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WUOS_PLUGIN_DIR
#define WUOS_PLUGIN_DIR ".wubuos/plugins"
#endif

struct WuOSPluginMgr {
    WuOSPluginEntry *e;
    int n, cap;
    WuOSHost host;
};

static void host_log(WuOSHost *h, const char *msg){
    (void)h;
    fprintf(stderr, "[plugin-host] %s\n", msg ? msg : "(null)");
}
static const char *host_get_setting(WuOSHost *h, const char *key){
    (void)h; (void)key;
    return NULL;
}

WuOSPluginMgr *wuos_plugins_load(const char *dir){
    WuOSPluginMgr *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->host.abi_version  = WUOS_PLUGIN_ABI_VERSION;
    m->host.log          = host_log;
    m->host.get_setting  = host_get_setting;

    const char *d = dir;
    char path[2048];
    if (!d){
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(path, sizeof path, "%s/%s", home, WUOS_PLUGIN_DIR);
        d = path;
    }

    DIR *dp = opendir(d);
    if (!dp) return m;   /* no plugins dir => empty manager (valid) */

    struct dirent *de;
    while ((de = readdir(dp))){
        const char *fn = de->d_name;
        size_t L = strlen(fn);
        if (L < 3 || strcmp(fn + L - 3, ".so") != 0) continue;

        char full[4096];
        snprintf(full, sizeof full, "%s/%s", d, fn);

        void *h = dlopen(full, RTLD_NOW | RTLD_LOCAL);
        if (!h){ fprintf(stderr, "[plugin] dlopen %s: %s\n", full, dlerror()); continue; }

        WuOSPlugin *(*initfn)(void) =
            (WuOSPlugin *(*)(void)) dlsym(h, "wuos_plugin_init");
        if (!initfn){
            fprintf(stderr, "[plugin] %s: missing wuos_plugin_init (%s)\n", full, dlerror());
            dlclose(h); continue;
        }
        WuOSPlugin *p = initfn();
        if (!p){ dlclose(h); continue; }
        if (p->abi_version != WUOS_PLUGIN_ABI_VERSION){
            fprintf(stderr, "[plugin] %s: abi %d != host %d -- skipped\n",
                    full, p->abi_version, WUOS_PLUGIN_ABI_VERSION);
            dlclose(h); continue;
        }
        if (p->init && p->init(&m->host) != 0){
            fprintf(stderr, "[plugin] %s: init rejected -- skipped\n", full);
            dlclose(h); continue;
        }
        if (m->n == m->cap){
            m->cap = m->cap ? m->cap * 2 : 8;
            m->e = realloc(m->e, (size_t)m->cap * sizeof *m->e);
        }
        m->e[m->n].plugin = p;
        m->e[m->n].handle = h;
        m->n++;
        fprintf(stderr, "[plugin] loaded '%s' v%s (%s)\n",
                p->name ? p->name : "?", p->version ? p->version : "?",
                p->description ? p->description : "");
    }
    closedir(dp);
    return m;
}

void wuos_plugins_free(WuOSPluginMgr *m){
    if (!m) return;
    for (int i = 0; i < m->n; i++){
        if (m->e[i].plugin && m->e[i].plugin->quit) m->e[i].plugin->quit();
        if (m->e[i].handle) dlclose(m->e[i].handle);
    }
    free(m->e);
    free(m);
}

int wuos_plugins_count(const WuOSPluginMgr *m){ return m ? m->n : 0; }

const WuOSPlugin *wuos_plugins_get(const WuOSPluginMgr *m, int i){
    if (!m || i < 0 || i >= m->n) return NULL;
    return m->e[i].plugin;
}

char *wuos_plugins_exec(WuOSPluginMgr *m, int i, const char *args){
    if (!m || i < 0 || i >= m->n) return NULL;
    WuOSPlugin *p = m->e[i].plugin;
    if (!p || !p->exec) return NULL;
    return p->exec(&m->host, args);
}

/* ---- host-side accessors (for tests / non-GUI hosts) ----
 * These maintain a process-global manager so headless callers (test_view)
 * can load + run a plugin without standing up the SDL window. */
static WuOSPluginMgr *g_test_mgr = NULL;

int wuos_plugin_load_path(const char *so){
    if (!g_test_mgr){
        g_test_mgr = calloc(1, sizeof *g_test_mgr);
        if (!g_test_mgr) return -1;
        g_test_mgr->host.abi_version = WUOS_PLUGIN_ABI_VERSION;
        g_test_mgr->host.log         = host_log;
        g_test_mgr->host.get_setting = host_get_setting;
    }
    void *h = dlopen(so, RTLD_NOW | RTLD_LOCAL);
    if (!h){ fprintf(stderr, "[plugin] dlopen %s: %s\n", so, dlerror()); return -1; }
    WuOSPlugin *(*initfn)(void) =
        (WuOSPlugin *(*)(void)) dlsym(h, "wuos_plugin_init");
    if (!initfn){ dlclose(h); return -1; }
    WuOSPlugin *p = initfn();
    if (!p || p->abi_version != WUOS_PLUGIN_ABI_VERSION){
        if (p) fprintf(stderr, "[plugin] %s: abi %d != %d\n", so,
                       p?p->abi_version:-1, WUOS_PLUGIN_ABI_VERSION);
        dlclose(h); return -1;
    }
    if (p->init && p->init(&g_test_mgr->host) != 0){ dlclose(h); return -1; }
    if (g_test_mgr->n == g_test_mgr->cap){
        g_test_mgr->cap = g_test_mgr->cap ? g_test_mgr->cap*2 : 8;
        g_test_mgr->e = realloc(g_test_mgr->e,
                                (size_t)g_test_mgr->cap * sizeof *g_test_mgr->e);
    }
    g_test_mgr->e[g_test_mgr->n].plugin = p;
    g_test_mgr->e[g_test_mgr->n].handle = h;
    g_test_mgr->n++;
    return 0;
}

int wuos_plugin_count(void){ return g_test_mgr ? g_test_mgr->n : 0; }

const char *wuos_plugin_name(int i){
    if (!g_test_mgr || i < 0 || i >= g_test_mgr->n) return NULL;
    return g_test_mgr->e[i].plugin->name;
}

char *wuos_plugin_run(int i, const char *args){
    return wuos_plugins_exec(g_test_mgr, i, args);
}
