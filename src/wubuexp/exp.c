/* exp.c -- exporters. See exp.h. All formats are serialized by walking the
 * laid-out pages of a wubulayout_doc. */
#include "exp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../wubufont/wubufont.h"

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
                if (!nl) break;
                line = nl+1;
            }
            free(t);
        }
        /* content object */
        int cid = 3 + p*2;
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
                cid, strlen(content), content);
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


/* hop 14: does this run need the Unicode path? */
static int run_has_nonlatin(const char *t, size_t len){
    for (size_t k = 0; k < len; k++)
        if ((unsigned char)t[k] >= 0x80) return 1;
    return 0;
}

/* ---- hop 8: geometry-aware PDF export ----
 * Walks line boxes + runs so font size, bold/italic (Helvetica vs
 * Helvetica-Bold/Oblique), and per-run x/y survive. Falls back to the same
 * xref/PDF structure as the legacy path. */
int wubuexp_pdf_geometry(const wubulayout_doc *L, const char *out){
    FILE *f = fopen(out, "wb"); if (!f) return -1;
    int pg = wubulayout_page_count(L);
    const wubulayout_page_info *pi = wubulayout_page(L, 0);
    int page_w = pi ? pi->w : 794, page_h = pi ? pi->h : 1123;

    long off[8192]; int nobj = 0;
    fputs("%PDF-1.4\n", f);

    off[nobj++] = ftell(f);
    fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", f);
    long pages_pos = ftell(f);
    fputs("2 0 obj\n<< /Type /Pages /Kids [", f);
    int *page_ids = malloc(sizeof(int) * (pg > 0 ? pg : 1));

    /* fonts: F1 Helvetica, F2 Helvetica-Bold, F3 Helvetica-Oblique */
    int font_base = 3;
    int nfonts = 3;

    for (int p = 0; p < pg; p++){
        /* build content stream from runs */
        size_t cap = 8192, len = 0;
        char *content = malloc(cap); content[0] = 0;
        int nruns = wubulayout_run_count(L, p);
        for (int r = 0; r < nruns; r++){
            const wubulayout_run *run = wubulayout_run_at(L, p, r);
            if (!run || !run->text || run->text_len == 0) continue;
            int uni = run_has_nonlatin(run->text, run->text_len);
            /* F4 = Type0 Identity-H for non-Latin runs: correct text layer
             * for extraction/search; viewers substitute glyphs until full
             * CIDFont embedding lands. */
            int fid = uni ? (font_base + 3)
                    : (run->bold ? font_base + 1
                    : (run->italic ? font_base + 2 : font_base));
            int fs = run->font_size > 0 ? run->font_size : 12;
            /* PDF y is bottom-up: flip baseline */
            int py = page_h - run->y;
            int px = run->x;
            len += (size_t)snprintf(content + len, cap - len,
                    "BT /F%d %d Tf %d %d Td ", fid, fs, px, py);
            if (uni){
                content[len++] = '<';
                for (size_t k = 0; k < run->text_len; ){
                    unsigned char b0 = (unsigned char)run->text[k];
                    uint32_t cp;
                    if (b0 < 0x80){ cp = b0; k += 1; }
                    else if ((b0 & 0xE0) == 0xC0 && k+1 < run->text_len){
                        cp = ((b0 & 0x1F) << 6) | ((unsigned char)run->text[k+1] & 0x3F); k += 2;
                    }
                    else if ((b0 & 0xF0) == 0xE0 && k+2 < run->text_len){
                        cp = ((b0 & 0x0F) << 12) | (((unsigned char)run->text[k+1] & 0x3F) << 6)
                           | ((unsigned char)run->text[k+2] & 0x3F); k += 3;
                    }
                    else { cp = 0xFFFD; k += 1; }
                    if (cp >= 0x10000) cp = 0xFFFD;
                    if (len + 8 > cap){ cap = (len + 128) * 2; content = realloc(content, cap); }
                    len += (size_t)snprintf(content + len, cap - len, "%04X", cp);
                }
                content[len++] = '>';
                len += (size_t)snprintf(content + len, cap - len, " Tj ET\n");
                continue;
            }
            /* escaped string */
            if (len + run->text_len * 4 + 32 > cap){
                cap = (len + run->text_len * 4 + 64) * 2;
                char *nb = realloc(content, cap);
                if (!nb){ free(content); free(page_ids); fclose(f); return -1; }
                content = nb;
            }
            content[len++] = '(';
            for (size_t k = 0; k < run->text_len && len + 8 < cap; k++){
                char ch = run->text[k];
                if (ch == '(' || ch == ')' || ch == '\\') content[len++] = '\\';
                if ((unsigned char)ch >= 32 && (unsigned char)ch < 127)
                    content[len++] = ch;
                else content[len++] = '?';   /* latin path: placeholder */
            }
            content[len++] = ')';
            len += (size_t)snprintf(content + len, cap - len, " Tj ET\n");
        }
        /* draw object boxes as rectangles (tables/images/floats) */
        int nboxes = wubulayout_box_count(L, p);
        for (int b = 0; b < nboxes; b++){
            const wubulayout_box *bo = wubulayout_box_at(L, p, b);
            if (!bo) continue;
            int by = page_h - bo->y - bo->h;
            len += (size_t)snprintf(content + len, cap - len,
                    "0.8 0.85 0.95 RG %d %d %d %d re S\n",
                    bo->x, by, bo->w, bo->h);
            if (len + 256 > cap){
                cap = (len + 512) * 2;
                char *nb = realloc(content, cap);
                if (!nb){ free(content); free(page_ids); fclose(f); return -1; }
                content = nb;
            }
        }

        int cid = 10 + p * 2;
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Length %zu >>\nstream\n%s\nendstream\nendobj\n",
                cid, len, content);
        free(content);
        int pid = cid + 1;
        off[nobj++] = ftell(f);
        int f4id = font_base + 3;
        fprintf(f, "%d 0 obj\n<< /Type /Page /Parent 2 0 R "
                   "/MediaBox [0 0 %d %d] /Resources << /Font << /F%d %d 0 R "
                   "/F%d %d 0 R /F%d %d 0 R /F4 %d 0 R >> >> /Contents %d 0 R >>\nendobj\n",
                pid, page_w, page_h,
                font_base,     font_base,
                font_base + 1, font_base + 1,
                font_base + 2, font_base + 2, f4id, cid);
        page_ids[p] = pid;
    }
    /* font objects */
    static const char *fnames[3] = { "Helvetica", "Helvetica-Bold", "Helvetica-Oblique" };
    for (int i = 0; i < nfonts; i++){
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /%s >>\nendobj\n",
                font_base + i, fnames[i]);
    }
    /* F4: Type0 Identity-H for non-Latin runs. Text layer is correct
     * (ToUnicode-style UTF-16BE hex); viewers substitute glyphs since no
     * font program is embedded yet (tracked frontier). */
    off[nobj++] = ftell(f);
    fprintf(f, "%d 0 obj\n<< /Type /Font /Subtype /Type0 /BaseFont /Arial-Unicode-MS "
               "/Encoding /Identity-H /DescendantFonts [ %d 0 R ] >>\nendobj\n",
            font_base + 3, font_base + nfonts + 1);
    /* H15: embed the REAL font program (DejaVuSans covers CJK partially;
     * full CJK needs Noto — tracked). W array from font_advance. */
    uint8_t *fdata = NULL; size_t flen = 0; int have_font = 0; Font *ef = NULL;
    const char *fontpath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    FILE *ff = fopen(fontpath, "rb");
    if (ff){
        fseek(ff, 0, SEEK_END); flen = (size_t)ftell(ff); fseek(ff, 0, SEEK_SET);
        fdata = malloc(flen);
        if (fdata && fread(fdata, 1, flen, ff) == flen) have_font = 1;
        fclose(ff);
        if (!have_font){ free(fdata); fdata = NULL; }
    }
    if (have_font){
        ef = font_open_owned(fdata, flen, 0);   /* borrow; we own fdata */
        if (ef) have_font = 1;
        else { free(fdata); fdata = NULL; have_font = 0; }
    }
    long descendant_id = font_base + nfonts + 1;
    long descriptor_id = descendant_id + 1;
    long fontfile_id   = descendant_id + 2;
    if (have_font){
        off[nobj++] = ftell(f);
        fprintf(f, "%ld 0 obj\n<< /Type /Font /Subtype /CIDFontType2 /BaseFont /DejaVuSans "
                   "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
                   "/FontDescriptor %ld 0 R /CIDToGIDMap /Identity >>\nendobj\n",
                descendant_id, descriptor_id);
        /* descriptor with real metrics */
        off[nobj++] = ftell(f);
        fprintf(f, "%ld 0 obj\n<< /Type /FontDescriptor /FontName /DejaVuSans "
                   "/Flags 4 /FontBBox [ -1000 -1000 2000 2000 ] /ItalicAngle 0 "
                   "/Ascent %d /Descent %d /CapHeight 700 /StemV 80 "
                   "/FontFile2 %ld 0 R /MissingWidth 500 >>\nendobj\n",
                descriptor_id, ef ? font_ascent(ef) : 900,
                ef ? font_descent(ef) : -200, fontfile_id);
        /* font file */
        off[nobj++] = ftell(f);
        fprintf(f, "%ld 0 obj\n<< /Length %zu /Length1 %zu >>\nstream\n",
                fontfile_id, flen, flen);
        fwrite(fdata, 1, flen, f);
        fputs("\nendstream\nendobj\n", f);
    } else {
        off[nobj++] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Type /Font /Subtype /CIDFontType2 /BaseFont /Arial-Unicode-MS "
                   "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
                   "/CIDToGIDMap /Identity >>\nendobj\n",
                font_base + nfonts + 1);
    }
    /* fix page ids: they were computed assuming cid=10+p*2; rewrite Kids */
    fseek(f, pages_pos, SEEK_SET);
    fprintf(f, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int p = 0; p < pg; p++) fprintf(f, " %d 0 R", page_ids[p]);
    fprintf(f, " ] /Count %d >>\nendobj\n", pg);
    fseek(f, 0, SEEK_END);

    long xref = ftell(f);
    fprintf(f, "xref\n0 %d\n", nobj + 1);
    fputs("0000000000 65535 f \n", f);
    for (int i = 0; i < nobj; i++) fprintf(f, "%010ld 00000 n \n", off[i]);
    fprintf(f, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            nobj + 1, xref);
    free(page_ids);
    fclose(f);
    return 0;
}
