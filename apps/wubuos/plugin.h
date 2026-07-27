/* plugin.h -- wubuos plugin loader (host side).
 *
 * Scans a directory for *.so plugin modules, dlopen()s each, resolves
 * wuos_plugin_init, validates the ABI version, calls init(), and keeps the
 * live modules for later exec() dispatch. Mirrors how Notepad++ loads its
 * plugin DLLs, but portable (dlopen) and C11-clean.
 */
#ifndef WUOS_PLUGIN_LOADER_H
#define WUOS_PLUGIN_LOADER_H
#include "wuos_plugin.h"

typedef struct WuOSPluginEntry {
    WuOSPlugin *plugin;
    void       *handle;   /* dlopen handle (NULL for built-ins) */
} WuOSPluginEntry;

typedef struct WuOSPluginMgr WuOSPluginMgr;

/* Load every *.so in `dir`. Pass NULL for the default (~/.wubuos/plugins).
 * Returns a manager even if the dir is absent (count == 0). Free with
 * wuos_plugins_free(). */
WuOSPluginMgr *wuos_plugins_load(const char *dir);
void           wuos_plugins_free(WuOSPluginMgr *m);

int                      wuos_plugins_count(const WuOSPluginMgr *m);
const WuOSPlugin        *wuos_plugins_get(const WuOSPluginMgr *m, int i);
/* Invoke plugin i's exec(); returns a malloc'd string (caller frees) or NULL. */
char                    *wuos_plugins_exec(WuOSPluginMgr *m, int i, const char *args);

/* ---- host-side accessors (non-GUI / test drivers) ----
 * Maintain a process-global manager so headless callers can load + run a
 * plugin without standing up the SDL window. */
int  wuos_plugin_count(void);               /* loaded module count */
const char *wuos_plugin_name(int i);        /* name of module i */
int  wuos_plugin_load_path(const char *so); /* dlopen an explicit .so */
char *wuos_plugin_run(int i, const char *args);  /* exec module i (malloc'd) */

#endif /* WUOS_PLUGIN_LOADER_H */
