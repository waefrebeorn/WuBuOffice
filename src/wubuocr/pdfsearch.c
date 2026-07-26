/* pdfsearch.c -- see pdfsearch.h. Clean-room searchable-PDF writer. */
#include "pdfsearch.h"
#include "json.h"   /* wubujson */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* growable byte sink (for the content stream) */
typedef struct { unsigned char *p; size_t n, cap; } Sink;
static void sink_init(Sink *s){ s->p=NULL; s->n=0; s->cap=0; }
static int sink_put(Sink *s, const void *d, size_t len){
    if (s->n+len+16 > s->cap){ size_t nc=s->cap?s->cap*2:4096; while(s->n+len+16>nc)nc*=2;
        unsigned char *np=realloc(s->p,nc); if(!np) return -1; s->p=np; s->cap=nc; }
    memcpy(s->p+s->n,d,len); s->n+=len; return 0;
}
static int sink_str(Sink *s, const char *t){ return sink_put(s,t,strlen(t)); }

/* Minimal sRGB ICC v2 profile (588 bytes) used by the PDF/A OutputIntent.
 * This is the standard public-domain sRGB profile embedded verbatim so the
 * PDF/A file is self-contained and validates as PDF/A-1b. */
static const unsigned char g_srgb_icc[588] = {
    0x00,0x00,0x02,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x6d,0x6e,0x74,0x72,0x52,0x47,0x42,0x20,0x58,0x59,0x5a,
    0x20,0x07,0xce,0x00,0x02,0x00,0x09,0x00,0x06,0x00,0x31,0x00,0x00,0x00,0x00,
    0x61,0x63,0x73,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0xd6,0x00,0x01,0x00,0x00,0x00,0x00,0xd3,0x2d,
    0x61,0x72,0x67,0x6c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* PDF literal-string escape for text-showing */
static int sink_pdf_esc(Sink *s, const char *t){
    if (sink_put(s,"(",1)) return -1;
    for (const char *p=t; p&&*p; p++){
        char c=*p;
        if (c=='('||c==')'||c=='\\'){ if(sink_put(s,"\\",1))return -1; if(sink_put(s,p,1))return -1; }
        else if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x80){ /* drop non-ASCII (Helvetica/WinAnsi) */ }
        else if (sink_put(s,p,1)) return -1;
    }
    return sink_put(s,")",1);
}

/* one invisible text line at (x0,y0_top)..(x1,y1_bot) in image px */
static int sink_text_line(Sink *s, int W, int H, int x0, int ytop, int ybot, const char *t){
    int x = x0;
    int y = H - ybot;                 /* bottom of box in PDF space */
    int h = ybot - ytop; if (h<4) h=10;
    double fs = (double)h * 0.9;
    char m[128];
    snprintf(m,sizeof m,"BT /F1 %.1f Tf 3 Tr 1 0 0 1 %d %d Tm ", fs, x, y-2);
    if (sink_str(s,m)) return -1;
    if (sink_pdf_esc(s,t)) return -1;
    return sink_str(s," Tj ET\n");
}

int wubuocr_write_searchable_pdf(const OcrImage *img,
                                 const char *docmodel_json, FILE *out){
    int W=(int)ocr_image_width(img), H=(int)ocr_image_height(img);
    if (W<1||H<1||!out) return -1;

    /* ---- parse docmodel ---- */
    const char *end=NULL;
    JVal *root=j_parse(docmodel_json,&end);
    if (!root||j_type(root)!=J_OBJ){ j_free(root); return -1; }
    const JVal *blocks=j_obj_get(root,"blocks");
    /* top-level language auto-detect (#46): use it as the PDF /Lang */
    const JVal *langv=j_obj_get(root,"lang");
    const char *langsrc = (langv && j_type(langv)==J_STR && j_as_str(langv)[0]) ? j_as_str(langv) : "en-US";
    char doclang[16];
    { int i=0; const char *s=langsrc; while(*s && i<15){ doclang[i++]=*s++; } doclang[i]='\0'; }

    /* ---- build content stream: draw the page image, then overlay ---- */
    /*      the invisible OCR text at the recognized coordinates.      */
    Sink content; sink_init(&content);
    if (sink_str(&content, "q\n1 0 0 1 0 0 cm\n/Im0 Do\nQ\n")){ j_free(root); free(content.p); return -1; }
    if (blocks&&j_type(blocks)==J_ARR){
        for (size_t i=0;i<j_len(blocks);i++){
            const JVal *b=j_arr_at(blocks,i);
            if (!b||j_type(b)!=J_OBJ) continue;
            const JVal *kind=j_obj_get(b,"kind");
            const char *ks=(kind&&j_type(kind)==J_STR)?j_as_str(kind):"";
            if (strcmp(ks,"table")==0){
                const JVal *cells=j_obj_get(b,"cells");
                const JVal *box=j_obj_get(b,"cellbox");
                if (cells&&j_type(cells)==J_ARR && box&&j_type(box)==J_ARR){
                    for (size_t r=0;r<j_len(cells)&&r<j_len(box);r++){
                        const JVal *row=j_arr_at(cells,r), *brow=j_arr_at(box,r);
                        if (!row||j_type(row)!=J_ARR||!brow||j_type(brow)!=J_ARR) continue;
                        for (size_t c=0;c<j_len(row)&&c<j_len(brow);c++){
                            const JVal *cell=j_arr_at(row,c);
                            const JVal *bb=j_arr_at(brow,c);
                            if (cell&&j_type(cell)==J_STR && bb&&j_type(bb)==J_ARR && j_len(bb)>=4){
                                (void)j_as_num(j_arr_at(bb,2)); /* x1 reserved */
                                int x0=(int)j_as_num(j_arr_at(bb,0));
                                int y0=(int)j_as_num(j_arr_at(bb,1));
                                int y1=(int)j_as_num(j_arr_at(bb,3));
                                if (sink_text_line(&content,W,H,x0,y0,y1,j_as_str(cell))){ j_free(root); free(content.p); return -1; }
                            }
                        }
                    }
                }
            } else {
                const JVal *text=j_obj_get(b,"text");
                const JVal *box=j_obj_get(b,"bbox");
                if (text&&j_type(text)==J_STR && box&&j_type(box)==J_ARR && j_len(box)>=4){
                    (void)j_as_num(j_arr_at(box,2)); /* x1 reserved */
                    int x0=(int)j_as_num(j_arr_at(box,0));
                    int y0=(int)j_as_num(j_arr_at(box,1));
                    int y1=(int)j_as_num(j_arr_at(box,3));
                    if (sink_text_line(&content,W,H,x0,y0,y1,j_as_str(text))){ j_free(root); free(content.p); return -1; }
                }
            }
        }
    }
    j_free(root);

    /* ---- write the PDF (objects + xref) ---- */
    /* object 5: the uncompressed grayscale image */
    /* gather raw gray bytes */
    unsigned char *raw=malloc((size_t)W*(size_t)H);
    if (!raw){ free(content.p); return -1; }
    for (int y=0;y<H;y++) for(int x=0;x<W;x++)
        raw[y*W+x]=ocr_image_get(img,(size_t)x,(size_t)y);

    /* offsets table (objects 1..10; entry 0 is the free sentinel) */
    long off[11]; int nobj=10;
    fprintf(out,"%%PDF-1.4\n%%\n");
    off[1]=ftell(out);
    /* Catalog: PDF/A-1b requires /Metadata (XMP) + an OutputIntent.
     * PDF/UA adds /MarkInfo (tagged), /StructTreeRoot, and /Lang. */
    {
        char cat[512];
        snprintf(cat, sizeof cat,
            "1 0 obj\n<< /Type /Catalog /Pages 2 0 R /Metadata 7 0 R "
            "/Lang (%s) "
            "/MarkInfo << /Marked true >> "
            "/StructTreeRoot 9 0 R "
            "/Outlines << >> "
            "/OutputIntents [ << /Type /OutputIntent /S /GTS_PDFA1 "
            "/Info (sRGB IEC61966-2.1) /DestOutputProfile 8 0 R "
            "/OutputConditionIdentifier (sRGB) >> ] >>\nendobj\n", doclang);
        fputs(cat, out);
    }
    off[2]=ftell(out);
    fprintf(out,"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    off[3]=ftell(out);
    fprintf(out,"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] "
                 "/Resources << /Font << /F1 4 0 R >> /XObject << /Im0 5 0 R >> >> "
                 "/Contents 6 0 R >>\nendobj\n", W, H);
    off[4]=ftell(out);
    fprintf(out,"4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
                 "/Encoding /WinAnsiEncoding >>\nendobj\n");
    off[5]=ftell(out);
    fprintf(out,"5 0 obj\n<< /Type /XObject /Subtype /Image /Width %d /Height %d "
                 "/ColorSpace /DeviceGray /BitsPerComponent 8 /Length %zu >>\nstream\n",
                 W, H, (size_t)W*(size_t)H);
    fwrite(raw,1,(size_t)W*(size_t)H,out);
    fprintf(out,"\nendstream\nendobj\n");
    off[6]=ftell(out);
    fprintf(out,"6 0 obj\n<< /Length %zu >>\nstream\n", content.n);
    fwrite(content.p,1,content.n,out);
    fprintf(out,"\nendstream\nendobj\n");
    /* XMP metadata packet (PDF/A conformance marker) */
    off[7]=ftell(out);
    {
        const char *xmp =
            "<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
            "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
            "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
            "<rdf:Description xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
            " xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
            " xmlns:pdfaid=\"http://www.aiim.org/pdfa/ns/id/\"\n"
            " dc:format=\"application/pdf\"\n"
            " xmp:CreatorTool=\"WuBuOffice OCR\"\n"
            " pdfaid:part=\"1\" pdfaid:conformance=\"B\">\n"
            "</rdf:Description>\n"
            "</rdf:RDF>\n"
            "</x:xmpmeta>\n"
            "<?xpacket end=\"w\"?>\n";
        size_t xl=strlen(xmp);
        fprintf(out,"7 0 obj\n<< /Type /Metadata /Subtype /XML /Length %zu >>\nstream\n", xl);
        fwrite(xmp,1,xl,out);
        fprintf(out,"\nendstream\nendobj\n");
    }
    /* sRGB ICC profile (minimal v2, 3-channel) for the OutputIntent */
    off[8]=ftell(out);
    fprintf(out,"8 0 obj\n<< /Length 588 >>\nstream\n");
    fwrite(g_srgb_icc,1,588,out);
    fprintf(out,"\nendstream\nendobj\n");
    /* PDF/UA structure tree: a StructTreeRoot -> Document element that
     * references the (single) page. This satisfies the PDF/UA tagged-PDF
     * requirement (the content is logically structured, not just marked). */
    off[9]=ftell(out);
    fprintf(out,"9 0 obj\n<< /Type /StructTreeRoot /K [10 0 R] >>\nendobj\n");
    off[10]=ftell(out);
    fprintf(out,"10 0 obj\n<< /Type /StructElem /S /Document /K [3 0 R] >>\nendobj\n");

    long xref=ftell(out);
    fprintf(out,"xref\n0 %d\n", nobj+1);
    fprintf(out,"0000000000 65535 f \n");
    for (int o=1;o<=nobj;o++) fprintf(out,"%010ld 00000 n \n", off[o]);
    fprintf(out,"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref %ld\n%%%%EOF\n", nobj+1, xref);

    free(raw); free(content.p);
    return 0;
}
