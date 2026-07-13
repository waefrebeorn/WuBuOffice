/* docx_document.c — WordprocessingML document.xml -> unified model
 * (ws05#0884 / ws05#082). Streaming SAX via wubuxml_parse,
 * maintaining a stack of open model nodes. */

#include "docx_document.h"
#include "../wubuxml/parser.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    wubumodel_doc *doc;
    wubumodel_node *stack[64];   /* open model nodes */
    int sp;
    int in_text;                  /* inside a <w:t> */
    char *textbuf;
    size_t tcap, tlen;
} ctx_t;

static wubumodel_node *top(ctx_t *c) {
    return c->sp > 0 ? c->stack[c->sp - 1] : NULL;
}
static void push(ctx_t *c, wubumodel_node *n) {
    if (c->sp < 64) c->stack[c->sp++] = n;
}
static void pop(ctx_t *c) {
    if (c->sp > 0) c->sp--;
}

/* flush accumulated text into a RUN under the top paragraph */
static void flush_text(ctx_t *c) {
    if (c->tlen == 0) return;
    wubumodel_node *par = top(c);
    if (!par || wubumodel_node_kind(par) != WUBUMODEL_PARAGRAPH) return;
    wubumodel_node *run = wubumodel_node_create(c->doc, WUBUMODEL_RUN);
    c->textbuf[c->tlen] = 0;
    wubumodel_run_set_text(run, c->textbuf);
    wubumodel_node_append(c->doc, par, run);
    c->tlen = 0;
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    ctx_t *c = user;

    if (evt == WUBUXML_EVT_TEXT) {
        if (c->in_text && info->text_len) {
            if (c->tlen + info->text_len + 1 > c->tcap) {
                c->tcap = (c->tlen + info->text_len) * 2 + 64;
                char *nb = realloc(c->textbuf, c->tcap);
                if (!nb) return -1;
                c->textbuf = nb;
            }
            memcpy(c->textbuf + c->tlen, info->text, info->text_len);
            c->tlen += info->text_len;
        }
        return 0;
    }

    const char *nm = info->name;
    int is_w = (strncmp(nm, "w:", 2) == 0);
    const char *local = is_w ? nm + 2 : nm;

    if (evt == WUBUXML_EVT_START) {
        if (is_w && strcmp(local, "body") == 0) {
            wubumodel_node *sec = wubumodel_node_create(c->doc, WUBUMODEL_SECTION);
            push(c, sec);
        } else if (is_w && strcmp(local, "tbl") == 0) {
            flush_text(c);
            wubumodel_node *tbl = wubumodel_node_create(c->doc, WUBUMODEL_TABLE);
            wubumodel_node *parent = top(c);
            if (parent) wubumodel_node_append(c->doc, parent, tbl);
            push(c, tbl);
        } else if (is_w && strcmp(local, "tr") == 0) {
            flush_text(c);
            wubumodel_node *row = wubumodel_node_create(c->doc, WUBUMODEL_CELL);
            wubumodel_node *parent = top(c);
            if (parent) wubumodel_node_append(c->doc, parent, row);
            push(c, row);
        } else if (is_w && strcmp(local, "tc") == 0) {
            flush_text(c);
            wubumodel_node *cell = wubumodel_node_create(c->doc, WUBUMODEL_CELL);
            wubumodel_node *parent = top(c);
            if (parent) wubumodel_node_append(c->doc, parent, cell);
            push(c, cell);
        } else if (is_w && (strcmp(local, "p") == 0)) {
            flush_text(c);
            wubumodel_node *par = wubumodel_node_create(c->doc, WUBUMODEL_PARAGRAPH);
            wubumodel_node *parent = top(c);
            if (parent) wubumodel_node_append(c->doc, parent, par);
            push(c, par);
        } else if (is_w && strcmp(local, "t") == 0) {
            c->in_text = 1;
            c->tlen = 0;
        }
        return 0;
    }

    /* EVT_END */
    if (is_w && strcmp(local, "t") == 0) {
        c->in_text = 0;
        flush_text(c);
    } else if (is_w && (strcmp(local, "p") == 0 ||
                          strcmp(local, "tc") == 0 ||
                          strcmp(local, "tr") == 0 ||
                          strcmp(local, "tbl") == 0 ||
                          strcmp(local, "body") == 0)) {
        flush_text(c);
        pop(c);
    }
    return 0;
}

int wubuoxml_docx_to_model(const uint8_t *xml, size_t len,
                            wubumodel_doc *doc) {
    if (!xml || !doc) return -1;
    ctx_t c;
    memset(&c, 0, sizeof c);
    c.doc = doc;
    int rc = wubuxml_parse(xml, len, on_event, &c);
    flush_text(&c);
    free(c.textbuf);
    return rc;
}
