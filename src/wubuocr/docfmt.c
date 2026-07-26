/* docfmt.c -- OCR docmodel JSON -> alternate serializations. See docfmt.h */
#include "docfmt.h"
#include "json.h"   /* bundled wubujson */
#include "../wubuzip/zip.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>   /* getpid, unlink */

/* ---- growable string buffer ---- */
typedef struct { char *p; size_t n, cap; int err; } Buf;
static void buf_init(Buf *b){ b->p=NULL; b->n=0; b->cap=0; b->err=0; }
static void buf_free(Buf *b){ free(b->p); b->p=NULL; b->n=b->cap=0; }
static int buf_put(Buf *b, const char *s, size_t len){
    if (b->err) return -1;
    if (b->n+len+1 > b->cap){ size_t nc=b->cap?b->cap*2:256; while(b->n+len+1>nc)nc*=2;
        char *np=realloc(b->p,nc); if(!np){b->err=1;return -1;} b->p=np; b->cap=nc; }
    memcpy(b->p+b->n,s,len); b->n+=len; b->p[b->n]=0; return 0;
}
static int buf_str(Buf *b, const char *s){ return s?buf_put(b,s,strlen(s)):0; }
static int buf_ch(Buf *b, char c){ return buf_put(b,&c,1); }

/* JSON-string escape (writes the quoted, escaped string). */
static void esc_json(Buf *b, const char *s){
    buf_ch(b,'"');
    for (const char *p=s; p && *p; p++){
        char c=*p;
        switch(c){
            case '"': buf_str(b,"\\\""); break;
            case '\\': buf_str(b,"\\\\"); break;
            case '\n': buf_str(b,"\\n"); break;
            case '\r': buf_str(b,"\\r"); break;
            case '\t': buf_str(b,"\\t"); break;
            case '\b': buf_str(b,"\\b"); break;
            case '\f': buf_str(b,"\\f"); break;
            default:
                if ((unsigned char)c < 0x20) { char t[8]; snprintf(t,sizeof t,"\\u%04x",c); buf_str(b,t); }
                else buf_ch(b,c);
        }
    }
    buf_ch(b,'"');
}

/* XML-text escape (for XML/HTML bodies). */
static void esc_xml(Buf *b, const char *s){
    for (const char *p=s; p && *p; p++){
        char c=*p;
        switch(c){
            case '&': buf_str(b,"&amp;"); break;
            case '<': buf_str(b,"&lt;"); break;
            case '>': buf_str(b,"&gt;"); break;
            case '"': buf_str(b,"&quot;"); break;
            default: buf_ch(b,c);
        }
    }
}
/* XML-attribute escape (no quotes inside). */
static void esc_attr(Buf *b, const char *s){
    for (const char *p=s; p && *p; p++){
        char c=*p;
        if (c=='"'||c=='<'||c=='&'||c=='>') esc_xml(b,&c);
        else if ((unsigned char)c < 0x20) { char t[8]; snprintf(t,sizeof t,"&#%d;",(unsigned char)c); buf_str(b,t); }
        else buf_ch(b,c);
    }
}

/* RFC-4180 CSV field (quote if needed, double internal quotes). */
static void csv_field(Buf *b, const char *s){
    int need_q = 0;
    for (const char *p=s; p && *p; p++) if (*p==','||*p=='"'||*p=='\n'||*p=='\r'){ need_q=1; break; }
    if (need_q) buf_ch(b,'"');
    for (const char *p=s; p && *p; p++){ if (*p=='"') buf_str(b,"\"\""); else buf_ch(b,*p); }
    if (need_q) buf_ch(b,'"');
}

/* ---- shared walk: iterate blocks, dispatching by kind ---- */
typedef void (*EmitBlock)(Buf *b, const JVal *blk, int first);

static char *walk_blocks(const char *json, EmitBlock fn){
    const char *end=NULL;
    JVal *root = j_parse(json, &end);
    if (!root || j_type(root)!=J_OBJ){ j_free(root); return NULL; }
    const JVal *blocks = j_obj_get(root,"blocks");
    if (!blocks || j_type(blocks)!=J_ARR){ j_free(root); return NULL; }
    Buf b; buf_init(&b);
    int first=1;
    for (size_t i=0;i<j_len(blocks);i++){
        const JVal *blk=j_arr_at(blocks,i);
        if (!blk || j_type(blk)!=J_OBJ) continue;
        fn(&b, blk, first);
        first=0;
    }
    j_free(root);
    if (b.err){ buf_free(&b); return NULL; }
    char *out = b.p ? b.p : malloc(1);
    if (!out){ buf_free(&b); return NULL; }
    return out;
}

/* ---- plain text ---- */
static void emit_text(Buf *b, const JVal *blk, int first){
    const JVal *kind=j_obj_get(blk,"kind");
    const char *ks = (kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
    if (strcmp(ks,"table")==0){
        const JVal *cells=j_obj_get(blk,"cells");
        if (cells && j_type(cells)==J_ARR){
            for (size_t r=0;r<j_len(cells);r++){
                const JVal *row=j_arr_at(cells,r);
                if (!row||j_type(row)!=J_ARR) continue;
                for (size_t c=0;c<j_len(row);c++){
                    if (c) buf_ch(b,'\t');
                    const JVal *cell=j_arr_at(row,c);
                    if (cell&&j_type(cell)==J_STR) buf_str(b,j_as_str(cell));
                }
                buf_ch(b,'\n');
            }
        }
    } else {
        const JVal *text=j_obj_get(blk,"text");
        if (text&&j_type(text)==J_STR){ if(!first) buf_ch(b,'\n'); buf_str(b,j_as_str(text)); }
    }
}
char *docfmt_to_text(const char *json){ return walk_blocks(json, emit_text); }

/* ---- TSV ---- */
static void emit_tsv(Buf *b, const JVal *blk, int first){
    (void)first;
    const JVal *kind=j_obj_get(blk,"kind");
    const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
    const JVal *cells=j_obj_get(blk,"cells");
    if (strcmp(ks,"table")==0 && cells && j_type(cells)==J_ARR){
        for (size_t r=0;r<j_len(cells);r++){
            const JVal *row=j_arr_at(cells,r);
            if (!row||j_type(row)!=J_ARR) continue;
            for (size_t c=0;c<j_len(row);c++){
                if (c) buf_ch(b,'\t');
                const JVal *cell=j_arr_at(row,c);
                if (cell&&j_type(cell)==J_STR) buf_str(b,j_as_str(cell));
            }
            buf_ch(b,'\n');
        }
    } else {
        const JVal *text=j_obj_get(blk,"text");
        if (text&&j_type(text)==J_STR){ buf_str(b,j_as_str(text)); buf_ch(b,'\n'); }
    }
}
char *docfmt_to_tsv(const char *json){ return walk_blocks(json, emit_tsv); }

/* ---- CSV ---- */
static void emit_csv(Buf *b, const JVal *blk, int first){
    (void)first;
    const JVal *kind=j_obj_get(blk,"kind");
    const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
    const JVal *cells=j_obj_get(blk,"cells");
    if (strcmp(ks,"table")==0 && cells && j_type(cells)==J_ARR){
        for (size_t r=0;r<j_len(cells);r++){
            const JVal *row=j_arr_at(cells,r);
            if (!row||j_type(row)!=J_ARR) continue;
            for (size_t c=0;c<j_len(row);c++){
                if (c) buf_ch(b,',');
                const JVal *cell=j_arr_at(row,c);
                csv_field(b, (cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
            }
            buf_ch(b,'\n');
        }
    } else {
        const JVal *text=j_obj_get(blk,"text");
        csv_field(b, (text&&j_type(text)==J_STR)?j_as_str(text):"");
        buf_ch(b,'\n');
    }
}
char *docfmt_to_csv(const char *json){ return walk_blocks(json, emit_csv); }

/* ---- JSONL (one JSON object per block) ---- */
static void emit_jsonl(Buf *b, const JVal *blk, int first){
    (void)first; (void)b; (void)blk; /* implemented directly in docfmt_to_jsonl */
}
char *docfmt_to_jsonl(const char *json){
    const char *end=NULL;
    JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return NULL; }
    const JVal *blocks=j_obj_get(root,"blocks");
    if (!blocks||j_type(blocks)!=J_ARR){ j_free(root); return NULL; }
    Buf _b; Buf *b=&_b; buf_init(b);
    for (size_t i=0;i<j_len(blocks);i++){
        const JVal *blk=j_arr_at(blocks,i);
        if (!blk||j_type(blk)!=J_OBJ) continue;
        const JVal *kind=j_obj_get(blk,"kind");
        const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
        buf_str(b,"{\"kind\":");
        esc_json(b,ks);
        const JVal *text=j_obj_get(blk,"text");
        if (text&&j_type(text)==J_STR){ buf_str(b,",\"text\":"); esc_json(b,j_as_str(text)); }
        const JVal *conf=j_obj_get(blk,"conf");
        if (conf&&j_type(conf)==J_NUM){ char t[32]; snprintf(t,sizeof t,",\"conf\":%g",j_as_num(conf)); buf_str(b,t); }
        const JVal *cells=j_obj_get(blk,"cells");
        if (cells&&j_type(cells)==J_ARR){
            buf_str(b,",\"rows\":");
            const JVal *rows=j_obj_get(blk,"rows"), *cols=j_obj_get(blk,"cols");
            char t[32]; snprintf(t,sizeof t,"%g",rows?j_as_num(rows):(double)j_len(cells));
            buf_str(b,t); buf_str(b,",\"cols\":");
            snprintf(t,sizeof t,"%g",cols?j_as_num(cols):(j_len(cells)?(double)j_len(j_arr_at(cells,0)):0));
            buf_str(b,t); buf_str(b,",\"cells\":[");
            for (size_t r=0;r<j_len(cells);r++){
                const JVal *row=j_arr_at(cells,r);
                if (r) buf_ch(b,',');
                buf_ch(b,'[');
                if (row&&j_type(row)==J_ARR){
                    for (size_t c=0;c<j_len(row);c++){
                        if (c) buf_ch(b,',');
                        const JVal *cell=j_arr_at(row,c);
                        esc_json(b,(cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
                    }
                }
                buf_ch(b,']');
            }
            buf_str(b,"]");
        }
        buf_str(b,"}\n");
    }
    j_free(root);
    if (b->err){ buf_free(b); return NULL; }
    return b->p?b->p:malloc(1);
}

/* ---- LaTeX ---- */
static void esc_latex_inline(Buf *b, const char *s){
    for (const char *p=s; p&&*p; p++){
        char c=*p;
        if (c=='\\'||c=='&'||c=='%'||c=='$'||c=='#'||c=='_'||c=='{'||c=='}'){ buf_ch(b,'\\'); buf_ch(b,c); }
        else if (c=='~') buf_str(b,"\\textasciitilde{}");
        else if (c=='^') buf_str(b,"\\textasciicircum{}");
        else buf_ch(b,c);
    }
}
static void emit_latex(Buf *b, const JVal *blk, int first){
    (void)first;
    const JVal *kind=j_obj_get(blk,"kind");
    const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
    if (strcmp(ks,"table")==0){
        const JVal *cells=j_obj_get(blk,"cells");
        const JVal *cols=j_obj_get(blk,"cols");
        int C = cols? (int)j_as_num(cols) : (cells&&j_len(cells)? (int)j_len(j_arr_at(cells,0)) : 1);
        buf_str(b,"\\begin{tabular}{"); for (int i=0;i<C;i++) buf_str(b,"l|"); buf_str(b,"}\n\\hline\n");
        if (cells&&j_type(cells)==J_ARR){
            for (size_t r=0;r<j_len(cells);r++){
                const JVal *row=j_arr_at(cells,r);
                if (!row||j_type(row)!=J_ARR) continue;
                for (size_t c=0;c<j_len(row);c++){
                    if (c) buf_str(b," & ");
                    const JVal *cell=j_arr_at(row,c);
                    esc_latex_inline(b,(cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
                }
                buf_str(b," \\\\\\\\n\\hline\n");
            }
        }
        buf_str(b,"\\end{tabular}\n");
    } else {
        const JVal *text=j_obj_get(blk,"text");
        buf_str(b,"\\paragraph*{");
        if (text&&j_type(text)==J_STR) esc_latex_inline(b,j_as_str(text));
        buf_str(b,"}\n\n");
    }
}
char *docfmt_to_latex(const char *json){ return walk_blocks(json, emit_latex); }

/* ---- RTF ---- */
static void emit_rtf(Buf *b, const JVal *blk, int first){
    const JVal *kind=j_obj_get(blk,"kind");
    const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
    if (strcmp(ks,"table")==0){
        const JVal *cells=j_obj_get(blk,"cells");
        if (cells&&j_type(cells)==J_ARR){
            for (size_t r=0;r<j_len(cells);r++){
                const JVal *row=j_arr_at(cells,r);
                if (!row||j_type(row)!=J_ARR) continue;
                for (size_t c=0;c<j_len(row);c++){
                    if (c) buf_str(b,"\\tab ");
                    const JVal *cell=j_arr_at(row,c);
                    if (cell&&j_type(cell)==J_STR) buf_str(b,j_as_str(cell));
                }
                buf_str(b,"\\par\n");
            }
        }
    } else {
        const JVal *text=j_obj_get(blk,"text");
        if (!first) buf_str(b,"\\par\n");
        if (text&&j_type(text)==J_STR) buf_str(b,j_as_str(text));
    }
}
char *docfmt_to_rtf(const char *json){
    Buf _b; Buf *b=&_b; buf_init(b);
    buf_str(b,"{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier;}}\n");
    buf_str(b,"\\viewkind4\\uc1\\pard\\f0\\fs20\n");
    /* emit blocks */
    const char *end=NULL; JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); buf_free(b); return NULL; }
    const JVal *blocks=j_obj_get(root,"blocks");
    if (blocks&&j_type(blocks)==J_ARR){
        int first=1;
        for (size_t i=0;i<j_len(blocks);i++){
            const JVal *blk=j_arr_at(blocks,i);
            if (!blk||j_type(blk)!=J_OBJ) continue;
            emit_rtf(b,blk,first); first=0;
        }
    }
    j_free(root);
    buf_str(b,"\\par\n}\n");
    if (b->err){ buf_free(b); return NULL; }
    return b->p?b->p:malloc(1);
}

/* ---- hOCR (HTML) ---- */
char *docfmt_to_hocr(const char *json){
    const char *end=NULL; JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return NULL; }
    const JVal *blocks=j_obj_get(root,"blocks");
    Buf _b; Buf *b=&_b; buf_init(b);
    buf_str(b,"<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\"><title>OCR</title></head>\n<body>\n");
    buf_str(b,"<div class=\"ocr_page\">\n");
    if (blocks&&j_type(blocks)==J_ARR){
        for (size_t i=0;i<j_len(blocks);i++){
            const JVal *blk=j_arr_at(blocks,i);
            if (!blk||j_type(blk)!=J_OBJ) continue;
            const JVal *kind=j_obj_get(blk,"kind");
            const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
            const JVal *conf=j_obj_get(blk,"conf");
            char cstr[32]; snprintf(cstr,sizeof cstr,"%.0f",conf?j_as_num(conf):100);
            if (strcmp(ks,"table")==0){
                const JVal *cells=j_obj_get(blk,"cells");
                buf_str(b,"<table class=\"ocr_table\">\n");
                if (cells&&j_type(cells)==J_ARR){
                    for (size_t r=0;r<j_len(cells);r++){
                        const JVal *row=j_arr_at(cells,r);
                        buf_str(b,"<tr class=\"ocr_line\">\n");
                        if (row&&j_type(row)==J_ARR){
                            for (size_t c=0;c<j_len(row);c++){
                                const JVal *cell=j_arr_at(row,c);
                                buf_str(b,"<td class=\"ocr_carea\" title=\"x_wconf ");
                                buf_str(b,cstr); buf_str(b,"\">");
                                esc_xml(b,(cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
                                buf_str(b,"</td>\n");
                            }
                        }
                        buf_str(b,"</tr>\n");
                    }
                }
                buf_str(b,"</table>\n");
            } else {
                const JVal *text=j_obj_get(blk,"text");
                buf_str(b,"<p class=\"ocr_par\" title=\"x_wconf ");
                buf_str(b,cstr); buf_str(b,"\">");
                esc_xml(b,(text&&j_type(text)==J_STR)?j_as_str(text):"");
                buf_str(b,"</p>\n");
            }
        }
    }
    buf_str(b,"</div>\n</body>\n</html>\n");
    j_free(root);
    if (b->err){ buf_free(b); return NULL; }
    return b->p?b->p:malloc(1);
}

/* ---- ALTO XML ---- */
char *docfmt_to_alto(const char *json){
    const char *end=NULL; JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return NULL; }
    const JVal *blocks=j_obj_get(root,"blocks");
    Buf _b; Buf *b=&_b; buf_init(b);
    buf_str(b,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    buf_str(b,"<alto xmlns=\"http://www.loc.gov/standards/alto/ns-v4\">\n");
    buf_str(b,"<Layout><Page WIDTH=\"0\" HEIGHT=\"0\">\n");
    if (blocks&&j_type(blocks)==J_ARR){
        int tb=1;
        for (size_t i=0;i<j_len(blocks);i++){
            const JVal *blk=j_arr_at(blocks,i);
            if (!blk||j_type(blk)!=J_OBJ) continue;
            const JVal *kind=j_obj_get(blk,"kind");
            const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
            const JVal *conf=j_obj_get(blk,"conf");
            char cstr[32]; snprintf(cstr,sizeof cstr,"%.0f",conf?j_as_num(conf):100);
            if (strcmp(ks,"table")==0){
                const JVal *cells=j_obj_get(blk,"cells");
                buf_str(b,"<TextBlock ID=\"Tb");
                char id[16]; snprintf(id,sizeof id,"%d",tb++); buf_str(b,id);
                buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\">\n");
                if (cells&&j_type(cells)==J_ARR){
                    for (size_t r=0;r<j_len(cells);r++){
                        const JVal *row=j_arr_at(cells,r);
                        buf_str(b,"<TextLine ID=\"Ln");
                        char lid[16]; snprintf(lid,sizeof lid,"%d",(int)(i*100+r));
                        buf_str(b,lid); buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\">\n");
                        if (row&&j_type(row)==J_ARR){
                            for (size_t c=0;c<j_len(row);c++){
                                const JVal *cell=j_arr_at(row,c);
                                buf_str(b,"<String ID=\"St");
                                char sid[16]; snprintf(sid,sizeof sid,"%d",(int)(i*1000+r*10+c));
                                buf_str(b,sid); buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\" CONTENT=\"");
                                esc_attr(b,(cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
                                buf_str(b,"\" WC=\""); buf_str(b,cstr); buf_str(b,"\"/>\n");
                            }
                        }
                        buf_str(b,"</TextLine>\n");
                    }
                }
                buf_str(b,"</TextBlock>\n");
            } else {
                const JVal *text=j_obj_get(blk,"text");
                buf_str(b,"<TextBlock ID=\"Tb");
                char id[16]; snprintf(id,sizeof id,"%d",tb++); buf_str(b,id);
                buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\">\n");
                buf_str(b,"<TextLine ID=\"Ln");
                char lid[16]; snprintf(lid,sizeof lid,"%d",(int)(i*100));
                buf_str(b,lid); buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\">\n");
                buf_str(b,"<String ID=\"St");
                char sid[16]; snprintf(sid,sizeof sid,"%d",(int)(i*1000));
                buf_str(b,sid); buf_str(b,"\" HPOS=\"0\" VPOS=\"0\" WIDTH=\"0\" HEIGHT=\"0\" CONTENT=\"");
                esc_attr(b,(text&&j_type(text)==J_STR)?j_as_str(text):"");
                buf_str(b,"\" WC=\""); buf_str(b,cstr); buf_str(b,"\"/>\n");
                buf_str(b,"</TextLine>\n</TextBlock>\n");
            }
        }
    }
    buf_str(b,"</Page></Layout>\n</alto>\n");
    j_free(root);
    if (b->err){ buf_free(b); return NULL; }
    return b->p?b->p:malloc(1);
}

/* ---- TEI (Text Encoding Initiative) XML ---- */
char *docfmt_to_tei(const char *json){
    const char *end=NULL; JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return NULL; }
    const JVal *blocks=j_obj_get(root,"blocks");
    Buf _b; Buf *b=&_b; buf_init(b);
    buf_str(b,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    buf_str(b,"<TEI xmlns=\"http://www.tei-c.org/ns/1.0\">\n");
    buf_str(b,"<text>\n<body>\n");
    if (blocks&&j_type(blocks)==J_ARR){
        for (size_t i=0;i<j_len(blocks);i++){
            const JVal *blk=j_arr_at(blocks,i);
            if (!blk||j_type(blk)!=J_OBJ) continue;
            const JVal *kind=j_obj_get(blk,"kind");
            const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
            if (strcmp(ks,"table")==0){
                const JVal *cells=j_obj_get(blk,"cells");
                buf_str(b,"<table>\n");
                if (cells&&j_type(cells)==J_ARR){
                    for (size_t r=0;r<j_len(cells);r++){
                        const JVal *row=j_arr_at(cells,r);
                        buf_str(b,"<row>\n");
                        if (row&&j_type(row)==J_ARR){
                            for (size_t c=0;c<j_len(row);c++){
                                const JVal *cell=j_arr_at(row,c);
                                buf_str(b,"<cell>");
                                esc_xml(b,(cell&&j_type(cell)==J_STR)?j_as_str(cell):"");
                                buf_str(b,"</cell>\n");
                            }
                        }
                        buf_str(b,"</row>\n");
                    }
                }
                buf_str(b,"</table>\n");
            } else {
                const JVal *text=j_obj_get(blk,"text");
                buf_str(b,"<p>");
                esc_xml(b,(text&&j_type(text)==J_STR)?j_as_str(text):"");
                buf_str(b,"</p>\n");
            }
        }
    }
    buf_str(b,"</body>\n</text>\n</TEI>\n");
    j_free(root);
    if (b->err){ buf_free(b); return NULL; }
    return b->p?b->p:malloc(1);
}

/* ---- Excel (.xlsx) ----
 * Minimal valid OOXML SpreadsheetML: a zip (via wubuzip) containing
 * [Content_Types].xml, _rels/.rels, xl/workbook.xml, xl/_rels/workbook.xml.rels,
 * xl/worksheets/sheet1.xml, and xl/sharedStrings.xml. Paragraphs become single
 * rows (one cell, column A); tables become row x col grids. Clean-room,
 * dependency-free. The buffer is written to a temp FILE then read back so the
 * wubuzip streaming writer can finalize the zip before we hand it to the caller. */
int docfmt_to_xlsx(const char *json, char **out, size_t *out_len){
    const char *end=NULL;
    JVal *root=j_parse(json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return -1; }
    const JVal *blocks=j_obj_get(root,"blocks");
    if (!blocks||j_type(blocks)!=J_ARR){ j_free(root); return -1; }

    /* gather rows: each row is a list of cell strings */
    typedef struct { char **c; int n; } Row;
    Row *rows=NULL; size_t nrows=0, capr=0;
    int rc=-1;
    /* shared strings */
    Buf ss; buf_init(&ss);

    for (size_t i=0;i<j_len(blocks);i++){
        const JVal *blk=j_arr_at(blocks,i);
        if (!blk||j_type(blk)!=J_OBJ) continue;
        const JVal *kind=j_obj_get(blk,"kind");
        const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
        if (strcmp(ks,"table")==0){
            const JVal *cells=j_obj_get(blk,"cells");
            if (cells&&j_type(cells)==J_ARR){
                for (size_t r=0;r<j_len(cells);r++){
                    const JVal *row=j_arr_at(cells,r);
                    if (!row||j_type(row)!=J_ARR) continue;
                    if (nrows>=capr){ capr=capr?capr*2:16; rows=realloc(rows,capr*sizeof(Row)); if(!rows){buf_free(&ss);j_free(root);return -1;} }
                    Row *R=&rows[nrows]; R->c=NULL; R->n=0;
                    for (size_t c=0;c<j_len(row);c++){
                        const JVal *cell=j_arr_at(row,c);
                        const char *t=(cell&&j_type(cell)==J_STR)?j_as_str(cell):"";
                        R->c=realloc(R->c,(R->n+1)*sizeof(char*)); if(!R->c){buf_free(&ss);j_free(root);return -1;}
                        R->c[R->n++]=strdup(t?t:"");
                    }
                    nrows++;
                }
            }
        } else {
            const JVal *text=j_obj_get(blk,"text");
            const char *t=(text&&j_type(text)==J_STR)?j_as_str(text):"";
            if (nrows>=capr){ capr=capr?capr*2:16; rows=realloc(rows,capr*sizeof(Row)); if(!rows){buf_free(&ss);j_free(root);return -1;} }
            Row *R=&rows[nrows]; R->c=malloc(sizeof(char*)); R->c[0]=strdup(t?t:""); R->n=1;
            nrows++;
        }
    }

    /* shared strings XML */
    buf_str(&ss,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    { char t[64]; snprintf(t,sizeof t,"<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"%zu\" uniqueCount=\"%zu\">",nrows?nrows:1,nrows?nrows:1); buf_str(&ss,t); }
    buf_str(&ss,"\n");
    for (size_t r=0;r<nrows;r++)
        for (int c=0;c<rows[r].n;c++){
            buf_str(&ss,"<si><t xml:space=\"preserve\">"); esc_xml(&ss,rows[r].c[c]); buf_str(&ss,"</t></si>\n");
        }
    buf_str(&ss,"</sst>\n");

    /* sheet1.xml */
    Buf sheet; buf_init(&sheet);
    buf_str(&sheet,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_str(&sheet,"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>\n");
    for (size_t r=0;r<nrows;r++){
        char rn[16]; snprintf(rn,sizeof rn,"%zu",r+1);
        buf_str(&sheet,"<row r=\""); buf_str(&sheet,rn); buf_str(&sheet,"\">\n");
        int idx=0;
        for (int c=0;c<rows[r].n;c++){
            char col[8]; int ci=c; char *cp=col+sizeof(col)-1; *cp--=0;
            do { *cp="ABCDEFGHIJKLMNOPQRSTUVWXYZ"[ci%26]; ci/=26; } while(ci>0);
            buf_str(&sheet,"<c r=\""); buf_str(&sheet,cp); buf_str(&sheet,rn); buf_str(&sheet,"\" t=\"s\"><v>");
            char vi[16]; snprintf(vi,sizeof vi,"%d",idx++); buf_str(&sheet,vi); buf_str(&sheet,"</v></c>\n");
        }
        buf_str(&sheet,"</row>\n");
    }
    buf_str(&sheet,"</sheetData></worksheet>\n");

    /* workbook.xml + rels + content types + root rels */
    const char *wb="<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"OCR\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>\n";
    const char *wbr="<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/></Relationships>\n";
    const char *ct="<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/><Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/></Types>\n";
    const char *rr="<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>\n";

    /* write the zip to a temp file, then read it back */
    char tp[256]; snprintf(tp,sizeof tp,"/tmp/wubuocr_xlsx_%d.zip",(int)getpid());
    FILE *tf=fopen(tp,"wb"); if(!tf){ goto cleanup; }
    wubuzip_writer *z=wubuzip_create(tf);
    if(!z){ fclose(tf); goto cleanup; }
    int ok=0;
    ok|=wubuzip_add(z,"[Content_Types].xml",ct,(uint32_t)strlen(ct));
    ok|=wubuzip_add(z,"_rels/.rels",rr,(uint32_t)strlen(rr));
    ok|=wubuzip_add(z,"xl/workbook.xml",wb,(uint32_t)strlen(wb));
    ok|=wubuzip_add(z,"xl/_rels/workbook.xml.rels",wbr,(uint32_t)strlen(wbr));
    ok|=wubuzip_add_deflated(z,"xl/worksheets/sheet1.xml",sheet.p,(uint32_t)sheet.n);
    ok|=wubuzip_add_deflated(z,"xl/sharedStrings.xml",ss.p,(uint32_t)ss.n);
    if (wubuzip_finalize(z)!=0) ok=1;
    fclose(tf);
    if (ok){ unlink(tp); goto cleanup; }
    /* read back */
    FILE *rf=fopen(tp,"rb"); if(!rf){ unlink(tp); goto cleanup; }
    fseek(rf,0,SEEK_END); long sz=ftell(rf); fseek(rf,0,SEEK_SET);
    char *buf=malloc(sz?(size_t)sz:1);
    if(!buf){ fclose(rf); unlink(tp); goto cleanup; }
    if (fread(buf,1,(size_t)sz,rf)!=(size_t)sz){ free(buf); fclose(rf); unlink(tp); goto cleanup; }
    fclose(rf); unlink(tp);
    *out=buf; *out_len=(size_t)sz; rc=0;

cleanup:
    for (size_t r=0;r<nrows;r++){ for(int c=0;c<rows[r].n;c++) free(rows[r].c[c]); free(rows[r].c); }
    free(rows);
    buf_free(&ss); buf_free(&sheet);
    j_free(root);
    return rc;
}
