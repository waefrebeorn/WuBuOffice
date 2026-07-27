/* test_plugin_abi.c -- headless verification of the wubuos plugin C ABI.
 * dlopens the sample plugin .so and walks load -> init -> exec, asserting the
 * host pointer crosses the .so boundary and exec returns the expected string.
 * Usage: test_plugin_abi <path-to-sample_plugin.so> */
#include "plugin.h"
#include "wuos_plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv){
    if (argc < 2){ fprintf(stderr, "usage: %s <sample_plugin.so>\n", argv[0]); return 2; }
    const char *so = argv[1];

    WuOSPluginMgr *m = wuos_plugins_load(NULL);  /* ~/.wubuos (likely empty) */
    if (!m){ fprintf(stderr, "mgr alloc failed\n"); return 1; }

    /* load the explicit sample .so via the test helper */
    if (wuos_plugin_load_path(so) != 0){
        fprintf(stderr, "[test] load_path %s FAILED\n", so);
        wuos_plugins_free(m);
        return 1;
    }
    int n = wuos_plugin_count();
    if (n != 1){ fprintf(stderr, "[test] count=%d want 1\n", n); wuos_plugins_free(m); return 1; }
    if (strcmp(wuos_plugin_name(0), "hello") != 0){
        fprintf(stderr, "[test] name='%s' want 'hello'\n", wuos_plugin_name(0));
        wuos_plugins_free(m); return 1;
    }
    char *r = wuos_plugin_run(0, NULL);
    if (!r){ fprintf(stderr, "[test] exec returned NULL\n"); wuos_plugins_free(m); return 1; }
    int ok = (strstr(r, "hello from hello v1.0.0 (host-ok)") != NULL);
    fprintf(stderr, "[test] exec -> '%s' (%s)\n", r, ok ? "ok" : "WRONG");
    free(r);

    wuos_plugins_free(m);
    return ok ? 0 : 1;
}
