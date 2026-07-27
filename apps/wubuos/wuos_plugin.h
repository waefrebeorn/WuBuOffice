/* wuos_plugin.h -- stable C ABI for wubuos plugins.
 *
 * A plugin is a shared object (.so / .dylib) that exports exactly one symbol,
 * `wuos_plugin_init`, returning a pointer to a static WuOSPlugin. The host
 * (the wubuos shell) loads it with dlopen, checks WUOS_PLUGIN_ABI_VERSION,
 * calls init(), and later dispatches exec() on user action.
 *
 * The ABI is intentionally small and forward-compatible: a WuOSHost pointer is
 * passed to init/exec so the host may later grow its callback table without
 * breaking already-built plugins (the version guard rejects mismatches).
 *
 * Plugins must include ONLY this header -- they never touch wubuos internals,
 * keeping the boundary clean and the ABI stable across host rebuilds.
 */
#ifndef WUOS_PLUGIN_H
#define WUOS_PLUGIN_H
#include <stddef.h>

#define WUOS_PLUGIN_ABI_VERSION 1

/* Export marker for the plugin's single entry point. */
#ifndef WUOS_PLUGIN_EXPORT
#  if defined(__GNUC__) || defined(__clang__)
#    define WUOS_PLUGIN_EXPORT __attribute__((visibility("default")))
#  elif defined(_MSC_VER)
#    define WUOS_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define WUOS_PLUGIN_EXPORT
#  endif
#endif

/* Opaque host context (filled by the loader; passed back to the plugin). */
typedef struct WuOSHost WuOSHost;

typedef struct WuOSPlugin {
    int         abi_version;     /* MUST equal WUOS_PLUGIN_ABI_VERSION */
    const char *name;            /* short id, e.g. "hello" */
    const char *version;         /* "1.0.0" */
    const char *description;     /* human-readable text */
    /* Called once after load. Return 0 to keep the plugin, non-zero to have
     * the host unload it immediately. `host` is valid for the plugin's life. */
    int  (*init)(WuOSHost *host);
    /* Invoked on user action (e.g. menu / key). `args` may be NULL. Returns a
     * malloc'd string the host frees, or NULL. */
    char *(*exec)(WuOSHost *host, const char *args);
    /* Called once on unload. May be NULL. */
    void (*quit)(void);
} WuOSPlugin;

/* The one exported symbol every plugin must provide. */
WUOS_PLUGIN_EXPORT WuOSPlugin *wuos_plugin_init(void);

/* Host callback table available to plugins through the WuOSHost pointer.
 * The layout is fixed for a given ABI_VERSION. */
struct WuOSHost {
    int abi_version;                       /* echoes WUOS_PLUGIN_ABI_VERSION */
    void (*log)(WuOSHost *h, const char *msg);          /* host logger */
    const char *(*get_setting)(WuOSHost *h, const char *key); /* optional cfg */
};

#endif /* WUOS_PLUGIN_H */
