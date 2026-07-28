/* pdfform.c -- PDF forms export (real AcroForm writer). See pdfform.h.
 *
 * Layout of the emitted PDF:
 *   obj 1: Catalog (/AcroForm -> obj 4)
 *   obj 2: Pages
 *   obj 3: Page (US-Letter, /Annots = field widgets)
 *   obj 4: AcroForm (/Fields array, /DA default appearance, /DR font)
 *   obj 5: Helvetica font
 *   obj 6..: one widget annotation per field (text field /Tx or checkbox /Btn)
 * xref offsets are byte-accurate; trailer /Root 1. */
#include "pdfform.h"
#include "form.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAXOBJ 300

typedef struct {
    char  *buf;
    size_t len, cap;
    size_t objoff[MAXOBJ];   /* byte offset of each object, 1-based */
    int    nobj;
} Pdf;

static int put(Pdf *p, const char *s){
    size_t l = strlen(s);
    while (p->len + l + 1 > p->cap){
        p->cap *= 2;
        char *nb = realloc(p->buf, p->cap);
        if (!nb) return 0;
        p->buf = nb;
    }
    memcpy(p->buf + p->len, s, l);
    p->len += l;
    p->buf[p->len] = 0;
    return 1;
}

static int beginobj(Pdf *p, int id){
    char t[32];
    if (id >= MAXOBJ) return 0;
    p->objoff[id] = p->len;
    if (id > p->nobj) p->nobj = id;
    snprintf(t, sizeof t, "%d 0 obj\n", id);
    return put(p, t);
}

/* Escape () \ in PDF strings. */
static void esc(const char *in, char *out, size_t cap){
    size_t o = 0;
    for (; *in && o + 2 < cap; in++){
        if (*in=='('||*in==')'||*in=='\\') out[o++]='\\';
        out[o++] = *in;
    }
    out[o] = 0;
}

int pdfform_build(const Form *form, uint8_t **out, size_t *out_len){
    if (!form || !out || !out_len) return 0;
    int nf = form_count(form);

    Pdf p; memset(&p, 0, sizeof p);
    p.cap = 8192; p.buf = malloc(p.cap);
    if (!p.buf) return 0;
    p.buf[0] = 0;

    put(&p, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");

    /* obj 1: Catalog */
    beginobj(&p, 1);
    put(&p, "<< /Type /Catalog /Pages 2 0 R /AcroForm 4 0 R >>\nendobj\n");

    /* obj 2: Pages */
    beginobj(&p, 2);
    put(&p, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");

    /* obj 3: Page with annots list (widgets are 6..6+nf-1) */
    beginobj(&p, 3);
    put(&p, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [");
    for (int i=0;i<nf;i++){
        char t[32]; snprintf(t, sizeof t, "%d 0 R ", 6+i); put(&p, t);
    }
    put(&p, "] >>\nendobj\n");

    /* obj 4: AcroForm */
    beginobj(&p, 4);
    put(&p, "<< /Fields [");
    for (int i=0;i<nf;i++){
        char t[32]; snprintf(t, sizeof t, "%d 0 R ", 6+i); put(&p, t);
    }
    put(&p, "] /DA (/Helv 12 Tf 0 g) /DR << /Font << /Helv 5 0 R >> >> /NeedAppearances true >>\nendobj\n");

    /* obj 5: font */
    beginobj(&p, 5);
    put(&p, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Name /Helv >>\nendobj\n");

    /* objs 6..: widgets */
    for (int i=0;i<nf;i++){
        const char *name = form_name_at(form, i);
        const char *val  = form_value(form, name);
        FormType ty      = form_type(form, name);
        int y = 700 - i*36;
        char nm[160], vv[560], t[1024];
        esc(name?name:"", nm, sizeof nm);
        esc(val?val:"",  vv, sizeof vv);
        beginobj(&p, 6+i);
        if (ty == FORM_CHECKBOX){
            int on = val && (strcmp(val,"on")==0 || strcmp(val,"yes")==0 || strcmp(val,"true")==0);
            snprintf(t, sizeof t,
                "<< /Type /Annot /Subtype /Widget /FT /Btn /T (%s) /Rect [72 %d 90 %d]"
                " /V /%s /AS /%s /F 4 >>\nendobj\n",
                nm, y, y+18, on?"Yes":"Off", on?"Yes":"Off");
        } else {
            snprintf(t, sizeof t,
                "<< /Type /Annot /Subtype /Widget /FT /Tx /T (%s) /V (%s)"
                " /Rect [72 %d 400 %d] /DA (/Helv 12 Tf 0 g) /F 4 >>\nendobj\n",
                nm, vv, y, y+24);
        }
        put(&p, t);
    }

    /* xref */
    size_t startxref = p.len;
    {
        char t[64];
        snprintf(t, sizeof t, "xref\n0 %d\n", p.nobj+1);
        put(&p, t);
        put(&p, "0000000000 65535 f \n");
        for (int i=1;i<=p.nobj;i++){
            snprintf(t, sizeof t, "%010zu 00000 n \n", p.objoff[i]);
            put(&p, t);
        }
        snprintf(t, sizeof t,
            "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
            p.nobj+1, startxref);
        put(&p, t);
    }

    *out = (uint8_t*)p.buf;
    *out_len = p.len;
    return 1;
}

int pdfform_write_file(const Form *form, const char *path){
    uint8_t *buf; size_t len;
    if (!path || !pdfform_build(form, &buf, &len)) return -1;
    FILE *f = fopen(path, "wb");
    if (!f){ free(buf); return -1; }
    size_t w = fwrite(buf, 1, len, f);
    fclose(f); free(buf);
    return w==len ? 0 : -1;
}
