/* test_pdfform.c -- real AcroForm PDF writer. */
#include "pdfform.h"
#include "form.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)

static const char *find(const uint8_t *h, size_t hl, const char *n){
    size_t nl = strlen(n);
    if (nl==0 || hl<nl) return NULL;
    for (size_t i=0;i+nl<=hl;i++)
        if (memcmp(h+i, n, nl)==0) return (const char*)(h+i);
    return NULL;
}

int main(void){
    Form *f = form_create();
    form_add(f, "fullname", FORM_TEXT, "Alice (Smith)");
    form_add(f, "agree", FORM_CHECKBOX, "on");

    uint8_t *buf; size_t len;
    CK(pdfform_build(f, &buf, &len)==1, "build");
    CK(buf && len>0, "non-empty");
    if (buf){
        CK(memcmp(buf, "%PDF-1.4", 8)==0, "pdf header");
        CK(find(buf,len,"/AcroForm")!=NULL, "acroform present");
        CK(find(buf,len,"/FT /Tx")!=NULL, "text field");
        CK(find(buf,len,"/FT /Btn")!=NULL, "checkbox field");
        CK(find(buf,len,"(fullname)")!=NULL, "field name");
        CK(find(buf,len,"(Alice \\(Smith\\))")!=NULL, "value escaped");
        CK(find(buf,len,"/V /Yes")!=NULL, "checkbox on");
        CK(find(buf,len,"startxref")!=NULL, "xref trailer");
        CK(find(buf,len,"%%EOF")!=NULL, "eof marker");
        /* xref offset sanity: the startxref number points at "xref" */
        const char *sx = find(buf,len,"startxref");
        if (sx){
            long off = atol(sx+10);
            CK(off>0 && (size_t)off<len && memcmp(buf+off,"xref",4)==0,
               "xref offset byte-accurate");
        }
        free(buf);
    }
    CK(pdfform_write_file(f, "/tmp/test_form.pdf")==0, "write file");
    form_destroy(f);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: pdfform (AcroForm Tx+Btn, escaped values, valid xref)\n");
    return 0;
}
