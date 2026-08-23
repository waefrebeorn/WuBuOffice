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
    /* grab-bag: while capturing an unknown element, record its events so the
     * construct can be preserved verbatim and re-emitted on save. */
    int capturing;                /* depth of unknown-element capture */
    int capture_depth0;           /* stack depth when capture started */
    char *capbuf; size_t capcap, caplen;
    char capname[64];
    wubumodel_node *float_node;   /* H3: open wp:anchor image */
    int anchor_cx, anchor_cy;     /* H3: extent in px */
    int float_side, float_wrap;   /* H3: side/wrap collected from anchor */
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

static void cap_put(ctx_t *c, const char *s, size_t n){
    if (c->caplen + n + 1 > c->capcap){
        c->capcap = (c->caplen + n) * 2 + 128;
        char *nb = realloc(c->capbuf, c->capcap);
        if (!nb) return;
        c->capbuf = nb;
    }
    memcpy(c->capbuf + c->caplen, s, n);
    c->caplen += n;
}
static void cap_start_tag(ctx_t *c, const wubuxml_info *info){
    char tag[512]; size_t o=0;
    int w = snprintf(tag,sizeof tag,"<%s", info->name);
    if (w>0) { cap_put(c,tag,(size_t)w); o=(size_t)w; }
    for (int i=0;i<info->attr_count && o<sizeof tag;i++){
        w = snprintf(tag,sizeof tag," %s=\"%s\"", info->attr_name[i], info->attr_val[i]);
        if (w>0){ cap_put(c,tag,(size_t)w); }
    }
    cap_put(c,">",1);
}
static void cap_end_tag(ctx_t *c, const char *name){
    char tag[256]; int w = snprintf(tag,sizeof tag,"</%s>", name);
    if (w>0) cap_put(c,tag,(size_t)w);
}

/* Known WordprocessingML block/inline elements the model understands. */
static int known_w(const char *local){
    return !strcmp(local,"document") || !strcmp(local,"body")
        || !strcmp(local,"tbl") || !strcmp(local,"tr")
        || !strcmp(local,"tc") || !strcmp(local,"p") || !strcmp(local,"t")
        || !strcmp(local,"r");
}

static int on_event(wubuxml_event evt, const wubuxml_info *info, void *user) {
    ctx_t *c = user;

    /* ---- grab-bag capture mode: preserve unknown constructs verbatim ---- */
    if (c->capturing > 0){
        if (evt == WUBUXML_EVT_START){
            cap_start_tag(c, info);
            /* nested unknown elements deepen; known children still recorded
             * as raw so nothing is lost. */
            c->capturing++;
            return 0;
        }
        if (evt == WUBUXML_EVT_TEXT){
            cap_put(c, info->text ? info->text : "", info->text_len);
            return 0;
        }
        /* EVT_END */
        cap_end_tag(c, info->name);
        c->capturing--;
        if (c->capturing == 0){
            /* emit FOREIGN node under the enclosing paragraph/section */
            flush_text(c);
            wubumodel_node *parent = top(c);
            if (parent){
                wubumodel_node *fn = wubumodel_node_create(c->doc, WUBUMODEL_FOREIGN);
                if (c->capbuf) c->capbuf[c->caplen] = 0;
                wubumodel_node_set_foreign(fn, c->capname,
                                           c->capbuf ? c->capbuf : "");
                wubumodel_node_append(c->doc, parent, fn);
            }
            c->caplen = 0;
        }
        return 0;
    }

    /* ---- H3: floating image anchor (wp:anchor) ----
     * Record side/wrap/extent from the anchor's attrs + children so the
     * layout can square-wrap body text around the image. The image node
     * itself is emitted when the anchor closes. */
    if (evt == WUBUXML_EVT_START
        && (strcmp(info->name, "wp:anchor") == 0
            || strcmp(info->name, "anchor") == 0)){
        flush_text(c);
        c->anchor_cx = c->anchor_cy = 0;
        c->float_side = 2; c->float_wrap = 1;   /* defaults: right, square */
        c->float_node = (wubumodel_node *)1;    /* collecting */
        return 0;
    }
    if (c->float_node && evt == WUBUXML_EVT_START){
        if (info->name && strcmp(info->name, "wp:positionH") == 0){
            for (int i = 0; i < info->attr_count; i++)
                if (strcmp(info->attr_name[i], "relativeFrom") == 0
                    && strstr(info->attr_val[i], "left"))
                    c->float_side = 1;
        }
        /* extent cx/cy are in EMUs: 914400 per inch, 96 px/inch */
        if (strcmp(info->name, "wp:extent") == 0 || strcmp(info->name, "extent") == 0){
            for (int i = 0; i < info->attr_count; i++){
                if (strcmp(info->attr_name[i], "cx") == 0)
                    c->anchor_cx = (int)(atoll(info->attr_val[i]) * 96 / 914400);
                else if (strcmp(info->attr_name[i], "cy") == 0)
                    c->anchor_cy = (int)(atoll(info->attr_val[i]) * 96 / 914400);
            }
        }
        else if (strcmp(info->name, "wp:wrapNone") == 0
              || strcmp(info->name, "wrapNone") == 0){
            wubumodel_node_set_float(c->float_node, 0, 0, 0, 0);
        }
        else if (strcmp(info->name, "wp:wrapTopAndBottom") == 0
              || strcmp(info->name, "wrapTopAndBottom") == 0){
            wubumodel_node_set_float(c->float_node, 0, 2, 0, 0);
        }
        return 0;   /* swallow anchor children from text flow */
    }
    if (c->float_node && evt == WUBUXML_EVT_END
        && (strcmp(info->name, "wp:anchor") == 0
            || strcmp(info->name, "anchor") == 0)){
        /* emit the floating image node now that extent/wrap are known */
        wubumodel_node *parent = top(c);
        if (parent){
            wubumodel_node *im = wubumodel_node_create(c->doc, WUBUMODEL_IMAGE);
            wubumodel_node_set_float(im, c->float_side, c->float_wrap,
                                     c->anchor_cx ? c->anchor_cx : 120,
                                     c->anchor_cy ? c->anchor_cy : 120);
            wubumodel_node_append(c->doc, parent, im);
        }
        c->float_node = NULL;
        return 0;
    }

    /* ---- H6b: merged cells (w:gridSpan / w:vMerge inside w:tcPr) ----
     * tcPr is swallowed by the property-wrapper rule below; intercept these
     * two first so the open CELL records its span geometry. */
    if (evt == WUBUXML_EVT_START && c->sp > 0
        && wubumodel_node_kind(top(c)) == WUBUMODEL_CELL){
        if (!strcmp(info->name, "w:gridSpan") || !strcmp(info->name, "gridSpan")){
            for (int i = 0; i < info->attr_count; i++)
                if (!strcmp(info->attr_name[i], "w:val")
                    || !strcmp(info->attr_name[i], "val")){
                    int sp = atoi(info->attr_val[i]);
                    if (sp > 1)
                        wubumodel_node_set_span(top(c), sp,
                                                wubumodel_node_vmerge(top(c)));
                }
            return 0;
        }
        if (!strcmp(info->name, "w:vMerge") || !strcmp(info->name, "vMerge")){
            int vm = 2;   /* absent val = "continue" per spec */
            for (int i = 0; i < info->attr_count; i++)
                if ((!strcmp(info->attr_name[i], "w:val")
                     || !strcmp(info->attr_name[i], "val"))
                    && !strcmp(info->attr_val[i], "restart"))
                    vm = 1;
            wubumodel_node_set_span(top(c),
                                    wubumodel_node_col_span(top(c)), vm);
            return 0;
        }
    }

    /* entering capture: a namespaced element we do not model */
    if (evt == WUBUXML_EVT_START && strncmp(info->name, "w:", 2) == 0
        && !known_w(info->name + 2)){
        /* skip pure-formatting markers that carry no content */
        const char *l = info->name + 2;
        if (!strcmp(l,"rPr") || !strcmp(l,"pPr") || !strcmp(l,"tblPr")
            || !strcmp(l,"tcPr") || !strcmp(l,"trPr") || !strcmp(l,"sectPr")
            || !strcmp(l,"bookmarkStart") || !strcmp(l,"bookmarkEnd")
            || !strcmp(l,"drawing")){   /* H3: handled by the anchor branch */
            return 0;   /* silently ignore property wrappers */
        }
        flush_text(c);
        c->capturing = 1;
        c->capture_depth0 = c->sp;
        snprintf(c->capname, sizeof c->capname, "%s", l);
        c->caplen = 0;
        cap_start_tag(c, info);
        return 0;
    }

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
