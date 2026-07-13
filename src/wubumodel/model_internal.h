#ifndef WUBUMODEL_INTERNAL_H
#define WUBUMODEL_INTERNAL_H
/* Internal struct definitions. DO NOT include from apps/extensions. */
#include "model.h"

typedef struct style_prop {
    char *name;
    char *value;
    struct style_prop *next;
} style_prop;

struct wubumodel_style {
    style_prop *props;          /* simple linked list; small N */
    int refcount;
};

/* command inverse stored on a stack */
typedef struct cmd_inv {
    wubumodel_id node;
    wubumodel_cmd_kind kind;
    char *before;               /* previous text (for SET_TEXT) */
    struct cmd_inv *prev;
} cmd_inv;

typedef struct obs {
    wubumodel_change_cb cb;
    void *user;
    struct obs *next;
} obs;

struct wubumodel_node {
    wubumodel_id id;
    wubumodel_kind kind;
    char *text;                 /* valid for RUN */
    wubumodel_style *style;     /* shared, may be NULL */
    struct wubumodel_node *next;/* bucket chain in doc->nodes */
    struct wubumodel_node *parent;     /* back-link (weak) */
    struct wubumodel_node *first_child;/* head of child list */
    struct wubumodel_node *next_sibling;/* next in parent's child list */
};

#define WUBUMODEL_BUCKETS 1024

struct wubumodel_doc {
    wubumodel_id next_id;
    wubumodel_node *nodes[WUBUMODEL_BUCKETS];
    cmd_inv *undo_top;
    obs *observers;
};
#endif
