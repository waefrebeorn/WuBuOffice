/* badabi_plugin.c -- negative-test plugin: reports a WRONG ABI version so the
 * loader must reject it. Never loaded in production; only used by
 * test_plugin_abi to verify the ABI guard rejects mismatches. Depends only on
 * wuos_plugin.h. */
#include "wuos_plugin.h"
#include <stdlib.h>

static WuOSPlugin g_plugin = {
    .abi_version = 999,            /* intentionally wrong -> loader rejects */
    .name        = "badabi",
    .version     = "0.0.0",
    .description = "negative-test plugin with a mismatched ABI version",
    .init        = NULL,
    .exec        = NULL,
    .quit        = NULL,
};

WUOS_PLUGIN_EXPORT WuOSPlugin *wuos_plugin_init(void){
    return &g_plugin;
}
