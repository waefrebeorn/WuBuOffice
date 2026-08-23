/* table_styles.c -- H6 implementation. See table_styles.h.
 * Parses styles.xml via the existing wubuxml SAX parser (same event API the
 * document.xml reader uses) and resolves basedOn chains. C11, no deps. */
#include "table_styles.h"
#include "../wubuxml/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    TableStyle *arr; size_t n, cap;
    TableStyle *cur;          /* style being parsed */
    int in_table_style;
    char cur_cond[16];        /* firstRow/lastRow/band1H/band2H or "" */
} Ctx;

static TblCellProps *cond_target(Ctx *c){
    if (!c->cur_cond[0]) return NULL;
    if (!strcmp(c->cur_cond,"firstRow")) return &c->cur->first_row;
    if (!strcmp(c->cur_cond,"lastRow"))  return &c->cur->last_row;
    if (!strcmp(c->cur_cond,"band1H"))   return &c->cur->band1h;
    if (!strcmp(c->cur_cond,"band2H"))   return &c->cur->band2h;
    return NULL;
}

static const char *attr(const wubuxml_info *in, const char *a, const char *wa){
    for (int i = 0; i < in->attr_count; i++)
        if (!strcmp(in->attr_name[i], a) || !strcmp(in->attr_name[i], wa))
            return in->attr_val[i];
    return NULL;
}

static int on_event(wubuxml_event evt, const wubuxml_info *in, void *user){
    Ctx *c = user;

    if (evt == WUBUXML_EVT_START){
        if (!strcmp(in->name,"w:style") || !strcmp(in->name,"style")){
            const char *type = attr(in,"w:type","type");
            const char *id   = attr(in,"w:styleId","styleId");
            c->in_table_style = type && id && !strcmp(type,"table");
            if (c->in_table_style){
                if (c->n == c->cap){
                    c->cap = c->cap ? c->cap*2 : 8;
                    c->arr = realloc(c->arr, c->cap * sizeof *c->arr);
                }
                TableStyle *t = &c->arr[c->n++];
                memset(t, 0, sizeof *t);
                snprintf(t->id, sizeof t->id, "%s", id);
                t->shading_r = t->shading_g = t->shading_b = -1;
                t->first_row.shading_r = t->first_row.shading_g = t->first_row.shading_b = -1;
                t->last_row.shading_r  = t->last_row.shading_g  = t->last_row.shading_b  = -1;
                t->band1h.shading_r    = t->band1h.shading_g    = t->band1h.shading_b    = -1;
                t->band2h.shading_r    = t->band2h.shading_g    = t->band2h.shading_b    = -1;
                c->cur = t;
            }
            return 0;
        }
        if (!c->in_table_style || !c->cur) return 0;

        if (!strcmp(in->name,"w:basedOn") || !strcmp(in->name,"basedOn")){
            const char *v = attr(in,"w:val","val");
            if (v) snprintf(c->cur->based_on, sizeof c->cur->based_on, "%s", v);
            return 0;
        }
        if (!strcmp(in->name,"w:tblStylePr") || !strcmp(in->name,"tblStylePr")){
            const char *t = attr(in,"w:type","type");
            if (!t) return 0;
            if      (!strcmp(t,"firstRow")){ c->cur->has_first_row = 1; snprintf(c->cur_cond,sizeof c->cur_cond,"firstRow"); }
            else if (!strcmp(t,"lastRow")) { c->cur->has_last_row  = 1; snprintf(c->cur_cond,sizeof c->cur_cond,"lastRow"); }
            else if (!strcmp(t,"band1H"))  { c->cur->has_band1h   = 1; snprintf(c->cur_cond,sizeof c->cur_cond,"band1H"); }
            else if (!strcmp(t,"band2H"))  { c->cur->has_band2h   = 1; snprintf(c->cur_cond,sizeof c->cur_cond,"band2H"); }
            return 0;
        }
        if (!strcmp(in->name,"w:b") || !strcmp(in->name,"b")){
            /* <w:b/> or <w:b w:val="0"/> */
            const char *v = attr(in,"w:val","val");
            int on = !v || !strcmp(v,"1") || !strcmp(v,"true");
            TblCellProps *tgt = cond_target(c);
            if (tgt) tgt->bold = on; else c->cur->bold = on;
            return 0;
        }
        if (!strcmp(in->name,"w:jc") || !strcmp(in->name,"jc")){
            const char *v = attr(in,"w:val","val");
            int centered = v && !strcmp(v,"center");
            TblCellProps *tgt = cond_target(c);
            if (tgt) tgt->centered = centered; else c->cur->centered = centered;
            return 0;
        }
        if (!strcmp(in->name,"w:shd") || !strcmp(in->name,"shd")){
            const char *fill = attr(in,"w:fill","fill");
            if (fill && strlen(fill) == 6){
                TblCellProps *tgt = cond_target(c);
                int r = strtol((char[]){fill[0],fill[1],0},0,16);
                int g = strtol((char[]){fill[2],fill[3],0},0,16);
                int b = strtol((char[]){fill[4],fill[5],0},0,16);
                if (tgt){ tgt->shading_r=r; tgt->shading_g=g; tgt->shading_b=b; }
                else { c->cur->shading_r=r; c->cur->shading_g=g; c->cur->shading_b=b; }
            }
            return 0;
        }
        return 0;
    }

    if (evt == WUBUXML_EVT_END){
        if (!strcmp(in->name,"w:style") || !strcmp(in->name,"style")){
            c->in_table_style = 0; c->cur = NULL; c->cur_cond[0] = 0;
        }
        else if (!strcmp(in->name,"w:tblStylePr") || !strcmp(in->name,"tblStylePr"))
            c->cur_cond[0] = 0;
    }
    return 0;
}

TableStyle *table_styles_parse(const char *styles_xml, size_t len, size_t *count){
    if (!styles_xml || !len || !count) return NULL;
    Ctx c; memset(&c, 0, sizeof c);
    int rc = wubuxml_parse((const uint8_t*)styles_xml, len, on_event, &c);
    if (rc != 0){ free(c.arr); *count = 0; return NULL; }
    *count = c.n;
    return c.arr;
}

void table_styles_free(TableStyle *arr){ free(arr); }

TblCellProps table_styles_resolve(const TableStyle *styles, size_t count,
                                  const char *style_id, unsigned look_flags,
                                  int row_index){
    TblCellProps p = {0,0,-1,-1,-1};
    /* collect the chain: derived -> base (walk up), then apply root-first so
     * more-derived styles override. Chain depth cap prevents cycles. */
    const TableStyle *chain[16];
    int depth = 0;
    const TableStyle *cur = NULL;
    for (size_t i = 0; i < count; i++)
        if (!strcmp(styles[i].id, style_id)){ cur = &styles[i]; break; }
    fprintf(stderr,"[dbgR] id=%s found=%d depth_walk\n", style_id, cur?1:0);
    while (cur && depth < 16){
        chain[depth++] = cur;
        if (!cur->based_on[0]) break;
        cur = NULL;
        for (size_t i = 0; i < count; i++)
            if (!strcmp(styles[i].id, chain[depth-1]->based_on)){
                /* self/cycle guard */
                int seen = 0;
                for (int k = 0; k < depth; k++) if (&styles[i] == chain[k]) seen = 1;
                if (!seen) cur = &styles[i];
                break;
            }
    }
    /* merge root -> derived */
    for (int d = depth - 1; d >= 0; d--){
        const TableStyle *st = chain[d];
        p.bold |= st->bold;
        p.centered |= st->centered;
        if (st->shading_r >= 0){ p.shading_r=st->shading_r; p.shading_g=st->shading_g; p.shading_b=st->shading_b; }
    }
    /* conditional overrides, most-derived wins per flag */
    for (int d = depth - 1; d >= 0; d--){
        const TableStyle *st = chain[d];
        int want_first = (look_flags & TS_LOOK_FIRST_ROW) && st->has_first_row && row_index == 0;
        int want_last  = (look_flags & TS_LOOK_LAST_ROW) && st->has_last_row;   /* caller passes row=-1 for last */
        int band_period = (look_flags & TS_LOOK_NO_HBAND) ? 0 : 1;
        int want_band = band_period
            && ((st->has_band1h && row_index > 0 && (row_index % 2) == 1)
             || (st->has_band2h && row_index > 0 && (row_index % 2) == 0));
        const TblCellProps *o = NULL;
        if (want_first) o = &st->first_row;
        else if (want_last) o = &st->last_row;
        else if (want_band) o = (row_index % 2) == 1 ? &st->band1h : &st->band2h;
        fprintf(stderr,"[dbgC] d=%d has_first=%d has_b2=%d want_first=%d want_band=%d o=%p\n",
                d, st->has_first_row, st->has_band2h, want_first, want_band, (void*)o);
        if (o){
            if (o->bold) p.bold = 1;
            if (o->centered) p.centered = 1;
            if (o->shading_r >= 0){ p.shading_r=o->shading_r; p.shading_g=o->shading_g; p.shading_b=o->shading_b; }
        }
    }
    return p;
}
