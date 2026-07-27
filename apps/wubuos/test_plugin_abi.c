/* test_plugin_abi.c -- headless verification of the wubuos plugin C ABI.
 * Exercises both the happy path (load -> init -> exec) and the negative paths
 * the loader must reject:
 *   - a plugin reporting a mismatched ABI version is refused (count stays 0)
 *   - a non-existent / un-openable .so is refused
 * Usage: test_plugin_abi <good.so> <badabi.so> */
#include "plugin.h"
#include "wuos_plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){ fprintf(stderr,"[FAIL] %s\n", msg); failures++; } \
                              else fprintf(stderr,"[ok] %s\n", msg); } while(0)

int main(int argc, char **argv){
    if (argc < 3){ fprintf(stderr, "usage: %s <good.so> <badabi.so>\n", argv[0]); return 2; }
    const char *good = argv[1];
    const char *bad  = argv[2];

    WuOSPluginMgr *m = wuos_plugins_load(NULL);  /* ~/.wubuos (likely empty) */
    if (!m){ fprintf(stderr, "mgr alloc failed\n"); return 1; }

    /* ---- happy path ---- */
    CHECK(wuos_plugin_load_path(good) == 0, "load good .so");
    CHECK(wuos_plugin_count() == 1, "exactly one plugin loaded");
    CHECK(strcmp(wuos_plugin_name(0), "hello") == 0, "loaded plugin name == hello");
    char *r = wuos_plugin_run(0, NULL);
    CHECK(r && strstr(r, "hello from hello v1.0.0 (host-ok)"), "exec returns host-verified string");
    free(r);

    /* ---- negative: mismatched ABI version must be rejected ---- */
    int before = wuos_plugin_count();
    CHECK(wuos_plugin_load_path(bad) != 0, "loader rejects mismatched abi version");
    CHECK(wuos_plugin_count() == before, "rejected plugin not added to manager");

    /* ---- negative: non-existent file must be refused ---- */
    CHECK(wuos_plugin_load_path("/no/such/plugin.so") != 0, "loader rejects missing .so");
    CHECK(wuos_plugin_count() == before, "missing .so not added to manager");

    wuos_plugins_free(m);
    fprintf(stderr, "done failures=%d\n", failures);
    return failures ? 1 : 0;
}
