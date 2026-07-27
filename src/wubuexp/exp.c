/* exp.c -- exporters. See exp.h. All formats are serialized by walking the
 * laid-out pages of a wubulayout_doc. */
#include "exp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- helpers ---- */
static void escape_html(FILE *f, const char *s){
    for (; s && *s; s++){
        switch(*s){
            case '&': fputs("&amp;", f); break;
            case '<': fputs("&lt;", f); break;
            case '>': fputs("&gt;", f); break;
            case '"': fputs("&quot;", f); break;
            default: fputc(*s, f);
        }
    }
}
static void escape_latex(FILE *f, const char *s){
    for (; s && *s; s++){
        switch(*s){
            case '\\': fputs("\\\\", f); break;
            case '{': fputs("\\{", f); break;
            case '}': fputs("\\}", f); break;
            case '$': fputs("\\$", f); break;
            case '&': fputs("\\&", f); break;
            case '%': fputs("\\%", f); break;
            case '#': fputs("\\#", f); break;
            case '^': fputs("\\^{}", f); break;
            case '_': fputs("\\_", f); break;
            case '~': fputs("\\~{}", f); break;
            default: fputc(*s, f);
        }
    }
}
static void escape_rtf(FILE *f, const char *s){
    for (; s && *s; s++){
        unsigned char c = (unsigned char)*s;
        if (c == '\\') fputs("\\\\", f);
        else if (c == '{') fputs("\\{", f);
        else if (c == '}') fputs("\\}", f);
        else if (c == '\n') fputs("\\par\n", f);
        else if (c < 128) fputc(c, f);
        else fprintf(f, "\\u%d?", (int)c); /* unicode escape (best-effort) */
    }
}

/* ---- Markdown ---- */
int wubuexp_markdown(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "w"); if (!f) return -1;
    int pg = wubulayout_page_count(L);
    for (int p=0; p<pg; p++){
        char *t = wubulayout_page_text(L, p);
        if (t){ fputs(t, f); fputc('\n', f); free(t); }
    }
    fclose(f);
    return 0;
}

/* ---- HTML ---- */
int wubuexp_html(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "w"); if (!f) return -1;
    fputs("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
          "<title>WuBuOffice export</title></head><body>\n", f);
    int pg = wubulayout_page_count(L);
    for (int p=0; p<pg; p++){
        fprintf(f, "<section class=\"page\" data-page=\"%d\">\n", p+1);
        /* reconstruct paragraphs from line breaks */
        char *t = wubulayout_page_text(L, p);
        if (t){
            /* emit each line as a <div>; blank line -> paragraph break */
            const char *line = t;
            int in_p = 0;
            while (*line){
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl-line) : strlen(line);
                char buf[4096]; if (ll>=sizeof buf) ll=sizeof buf-1;
                memcpy(buf, line, ll); buf[ll]=0;
                if (ll==0){ if (in_p){ fputs("</p>\n", f); in_p=0; } }
                else { if (!in_p){ fputs("<p>", f); in_p=1; } escape_html(f, buf); }
                if (!nl) break;
                line = nl+1;
            }
            if (in_p) fputs("</p>\n", f);
            free(t);
        }
        fputs("</section>\n", f);
    }
    fputs("</body></html>\n", f);
    fclose(f);
    return 0;
}

/* ---- LaTeX ---- */
int wubuexp_latex(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "w"); if (!f) return -1;
    fputs("\\documentclass{article}\n\\begin{document}\n", f);
    int pg = wubulayout_page_count(L);
    for (int p=0; p<pg; p++){
        char *t = wubulayout_page_text(L, p);
        if (t){
            const char *line = t;
            while (*line){
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl-line) : strlen(line);
                char buf[4096]; if (ll>=sizeof buf) ll=sizeof buf-1;
                memcpy(buf, line, ll); buf[ll]=0;
                if (ll) { fputs("\\paragraph*{}\\noindent ", f); escape_latex(f, buf); fputc('\n', f); }
                if (!nl) break;
                line = nl+1;
            }
            free(t);
        }
    }
    fputs("\\end{document}\n", f);
    fclose(f);
    return 0;
}

/* ---- RTF ---- */
int wubuexp_rtf(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "w"); if (!f) return -1;
    fputs("{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil Helvetica;}}\n", f);
    int pg = wubulayout_page_count(L);
    for (int p=0; p<pg; p++){
        char *t = wubulayout_page_text(L, p);
        if (t){
            const char *line = t;
            while (*line){
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl-line) : strlen(line);
                char buf[4096]; if (ll>=sizeof buf) ll=sizeof buf-1;
                memcpy(buf, line, ll); buf[ll]=0;
                fputs("\\pard\\f0 ", f);
                escape_rtf(f, buf);
                fputs("\\par\n", f);
                if (!nl) break;
                line = nl+1;
            }
            free(t);
        }
    }
    fputs("}\n", f);
    fclose(f);
    return 0;
}

/* ---- PDF (from-scratch, base-14 Helvetica, one text block per page) ---- */
int wubuexp_pdf(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "wb"); if (!f) return -1;
    int pg = wubulayout_page_count(L);
    const wubulayout_page_info *pi = wubulayout_page(L, 0);
    int page_h = pi ? pi->h : 1123;

    /* collect object offsets as we write; PDF structure:
     * 1 catalog, 2 pages, then per page: [content, page] objects. */
    long off[4096]; int nobj=0;
    char header[16]; int hl = snprintf(header,sizeof header,"%%PDF-1.4\n");
    fwrite(header,1,hl,f);

    off[nobj++] = ftell(f);
    fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", f);

    off[nobj++] = ftell(f);
    /* pages object: Kids array filled after we know page object ids */
    long pages_obj_pos = ftell(f);
    fputs("2 0 obj\n<< /Type /Pages /Kids [", f);
    int *page_ids = malloc(sizeof(int)*pg);
    int content_ids[4096];
    for (int p=0;p<pg;p++){
        /* content stream */
        char *t = wubulayout_page_text(L, p);
        /* build content: BT /F1 size Tf x y Td (text) Tj ET, one line each */
        size_t cap=4096, len=0; char *content=malloc(cap); content[0]=0;
        if (t){
            const char *line=t;
            int y = page_h - 72;
            while (*line){
                const char *nl=strchr(line,'\n');
                size_t ll=nl?(size_t)(nl-line):strlen(line);
                char buf[4096]; if(ll>=sizeof buf) ll=sizeof buf-1;
                memcpy(buf,line,ll); buf[ll]=0;
                /* escape ( ) \ in PDF strings */
                char esc[8192]; int ei=0;
                for (size_t k=0;k<ll && ei<(int)sizeof esc-8;k++){
                    char c=buf[k];
                    if (c=='('||c==')'||c=='\\'){ esc[ei++]='\\'; }
                    esc[ei++]=c;
                }
                esc[ei]=0;
                size_t need=len+strlen(esc)+64;
                if(need>=cap){cap=need*2;char*nb=realloc(content,cap);if(!nb){free(content);free(t);free(page_ids);fclose(f);return -1;}content=nb;}
                len += (size_t)snprintf(content+len, cap-len, "BT /F1 12 Tf 72 %d Td (%s) Tj ET\n", y, esc);
                y -= 16;
                if(!nl) break; line=nl+1;
            }
            free(t);
        }
        /* content object */
        int cid = 3 + p*2;
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
                cid, strlen(content), content);
        content_ids[p]=cid;
        free(content);
        /* page object */
        int pid = cid+1;
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] "
                    "/Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R >>\nendobj\n",
                pid, pi?pi->w:794, page_h, 3+pg*2, cid); /* font obj = 3+pg*2 */
        page_ids[p]=pid;
    }
    /* font object */
    int font_id = 3 + pg*2;
    off[nobj++] = ftell(f);
    fprintf(f, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n", font_id);

    /* now write the Kids array into the pages object */
    fseek(f, pages_obj_pos, SEEK_SET);
    fprintf(f, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int p=0;p<pg;p++) fprintf(f, " %d 0 R", page_ids[p]);
    fprintf(f, " ] /Count %d >>\nendobj\n", pg);
    fseek(f, 0, SEEK_END);

    /* xref */
    long xref = ftell(f);
    fprintf(f, "xref\n0 %d\n", nobj+1);
    fputs("0000000000 65535 f \n", f);
    for (int i=0;i<nobj;i++) fprintf(f, "%010ld 00000 n \n", off[i]);
    fprintf(f, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", nobj+1, xref);
    free(page_ids);
    fclose(f);
    return 0;
}
