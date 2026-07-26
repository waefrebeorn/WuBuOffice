/* test_epub.c -- wubuepub acceptance test (valid EPUB3 container). */
#include "epub.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

static wubumodel_doc *make_doc(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec  = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "Hello EPUB world.");
    wubumodel_node_append(d, para, run);
    wubumodel_node_append(d, sec, para);
    return d;
}

/* read whole file into malloc'd buffer */
static char *read_file(const char *p, long *out_len){
    FILE *f = fopen(p, "rb"); if(!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f,0,SEEK_SET);
    char *b = malloc((size_t)n + 1); size_t got = fread(b,1,(size_t)n,f); fclose(f);
    b[got] = 0; *out_len = (long)got; return b;
}

int main(void){
    const char *ep = "/tmp/wubu_ep_test.epub";
    unlink(ep);

    wubumodel_doc *d = make_doc();
    int rc = epub_write(d, ep, "My Book", "en");
    CHECK(rc == 0, "epub_write ok");
    wubumodel_doc_destroy(d);

    /* file exists */
    long sz; char *raw = read_file(ep, &sz);
    CHECK(raw != NULL && sz > 0, "epub file produced");

    if (raw){
        /* EPUB requires "mimetype" as the FIRST entry, stored, containing
         * exactly "application/epub+zip". The precise check is done in the
         * member scan below (mime_ok), since binary ZIP bytes break strstr. */
        CHECK(sz >= 38 && memcmp(raw, "PK\x03\x04", 4) == 0, "zip local-file signature");

        /* Unzip and check required members via a tiny inline scan:
         * every local header starts with PK\x03\x04 and a filename follows. */
        int found_container=0, found_opf=0, found_nav=0, found_chap=0;
        int mime_ok=0;
        const unsigned char *p = (const unsigned char*)raw;
        size_t off = 0;
        while (off + 30 <= (size_t)sz && memcmp(p+off, "PK\x03\x04", 4)==0){
            uint16_t nlen = p[off+26] | (p[off+27]<<8);
            if (off + 30 + (size_t)nlen > (size_t)sz) break;
            const char *name = (const char*)(p + off + 30);
            uint32_t csize = p[off+18] | (p[off+19]<<8) | (p[off+20]<<16) | ((uint32_t)p[off+21]<<24);
            uint32_t elen = p[off+28] | (p[off+29]<<8);
            size_t data_off = off + 30 + (size_t)nlen + (size_t)elen;
            #define IS(name_, exp_) (nlen==strlen(exp_) && memcmp(name_, exp_, nlen)==0)
            if (IS(name, "mimetype")){
                /* mimetype must be STORED (uncompressed) and read 'application/epub+zip' */
                if (data_off + csize <= (size_t)sz && csize==20 &&
                    memcmp(p+data_off, "application/epub+zip", 20)==0) mime_ok=1;
            } else if (IS(name, "META-INF/container.xml")) found_container=1;
            else if (IS(name, "OEBPS/content.opf")) found_opf=1;
            else if (IS(name, "OEBPS/nav.xhtml")) found_nav=1;
            else if (IS(name, "OEBPS/chap_1.xhtml")) found_chap=1;
            #undef IS
            size_t next = data_off + (size_t)csize;
            if (next <= off) break; /* safety */
            off = next;
        }
        CHECK(mime_ok, "mimetype STORED + 'application/epub+zip'");
        CHECK(found_container, "has META-INF/container.xml");
        CHECK(found_opf, "has OEBPS/content.opf");
        CHECK(found_nav, "has OEBPS/nav.xhtml");
        CHECK(found_chap, "has OEBPS/chap_1.xhtml");
        free(raw);
    }

    /* NULL guards */
    CHECK(epub_write(NULL, ep, NULL, NULL) == -1, "NULL doc -> -1");

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuepub (valid EPUB3: mimetype + container + opf + nav + chapter)\n");
    return 0;
}
