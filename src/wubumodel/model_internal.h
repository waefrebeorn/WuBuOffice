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
    char *note;                 /* FOOTNOTE/ENDNOTE body (DOC-55) */
    char *link;                 /* LINK href/target (DOC-60) */
    uint8_t *img;               /* IMAGE embedded RGBA (DOC-61) */
    int img_w, img_h;           /* IMAGE dimensions */
    char *author;               /* COMMENT/TRACKCHANGE author (DOC-63/64) */
    char *field;                /* FIELD kind/value, e.g. "date" (DOC-65) */
    int tc;                     /* TRACKCHANGE type: 0 insert,1 delete (DOC-64) */
    int brk;                    /* PAGEBREAK/SECTIONBREAK type (DOC-57) */
    int float_side;             /* IMAGE: 0 inline, 1 left, 2 right (H3) */
    int float_wrap;             /* IMAGE: 0 none, 1 square, 2 top-bottom (H3) */
    int col_span;               /* CELL: grid columns spanned (>=1; H6b) */
    int vmerge;                 /* CELL: 0 none, 1 restart, 2 continue (H6b) */
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
