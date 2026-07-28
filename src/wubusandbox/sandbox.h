/* sandbox.h -- plugin capability sandbox (SCR-100). Sits over the plugin
 * C-ABI seed: each loaded plugin declares the capabilities it wants
 * (read/write doc, filesystem, network); the host grants a subset; every
 * privileged call is checked against the grant mask. Deny-by-default. Opaque. */
#ifndef WUBUSANDBOX_H
#define WUBUSANDBOX_H

typedef struct Sandbox Sandbox;

enum {
    SBX_READ_DOC  = 1<<0,
    SBX_WRITE_DOC = 1<<1,
    SBX_FS        = 1<<2,
    SBX_NET       = 1<<3,
};

Sandbox *sandbox_create(void);
void     sandbox_destroy(Sandbox *s);

/* Register a plugin with the caps it REQUESTS. Returns a plugin id >=0. */
int  sandbox_register(Sandbox *s, const char *name, unsigned requested);
/* Host grants caps (intersection with requested is what takes effect). */
int  sandbox_grant(Sandbox *s, int plugin_id, unsigned granted);
/* Check a call: 1 allowed, 0 denied. Denials are counted. */
int  sandbox_check(Sandbox *s, int plugin_id, unsigned cap);
/* Effective cap mask (requested AND granted). */
unsigned sandbox_effective(const Sandbox *s, int plugin_id);
int  sandbox_denials(const Sandbox *s, int plugin_id);
const char *sandbox_name(const Sandbox *s, int plugin_id);

#endif /* WUBUSANDBOX_H */
