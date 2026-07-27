/* sample_plugin.c -- reference wubuos plugin.
 *
 * Builds to sample_plugin.so. Demonstrates the full ABI: it reports its
 * metadata, uses the host log callback across the .so boundary during init,
 * and exec() returns a string proving the host pointer reached it intact.
 * Copy this file as a template for new plugins -- it depends ONLY on
 * wuos_plugin.h.
 */
#include "wuos_plugin.h"
#include <stdlib.h>
#include <string.h>

static int g_inited = 0;

static int hello_init(WuOSHost *host){
    g_inited = 1;
    if (host && host->log) host->log(host, "sample_plugin: init ok");
    return 0;   /* 0 => keep loaded */
}

static char *hello_exec(WuOSHost *host, const char *args){
    (void)args;
    if (host && host->log) host->log(host, "sample_plugin: exec");

    const char *body = (host && g_inited)
        ? "hello from hello v1.0.0 (host-ok)"
        : "hello: no-host";
    char *r = malloc(strlen(body) + 1);
    if (r) memcpy(r, body, strlen(body) + 1);
    return r;
}

static void hello_quit(void){
    if (g_inited) { g_inited = 0; }
}

static WuOSPlugin g_plugin = {
    .abi_version = WUOS_PLUGIN_ABI_VERSION,
    .name        = "hello",
    .version     = "1.0.0",
    .description = "Sample wubuos plugin demonstrating the C ABI.",
    .init        = hello_init,
    .exec        = hello_exec,
    .quit        = hello_quit,
};

WUOS_PLUGIN_EXPORT WuOSPlugin *wuos_plugin_init(void){
    return &g_plugin;
}
