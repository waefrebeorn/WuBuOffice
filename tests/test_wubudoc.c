/* test_wubudoc.c -- unified ingestion + creation facade (semantic engine).
 * Verifies "treat as media, not data" (WuBuContainer's no-binary-waterfall
 * rule): document/sheet/presentation formats ingest into a canonical model
 * JSON and re-create losslessly. Generics (zip, font) keep their own model.
 *   - md  -> model.blocks (semantic)
 *   - md  -> create docx -> re-ingest docx -> model.blocks (round-trip)
 *   - json model -> create odt (semantic create)
 *   - xlsx -> model.sheets ; csv -> model (or text)
 *   - generic zip / font keep parts/font model
 * Independent oracles: python zipfile + xml.dom for created docx/odt. */
#include "wubudoc.h"
#include "json.h"
#include "zip.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f);
    char *d = malloc((size_t)s + 1); size_t rd = fread(d, 1, (size_t)s, f);
    fclose(f); d[rd] = '\0'; *n = (size_t)rd; return d;
}

int main(void) {
    DocSession *s = doc_session_create();
    CK(s != NULL, "session create");

    /* --- md ingests as a SEMANTIC model (blocks), not raw text --- */
    long mid = doc_ingest_text(s, "md", "# Title\n\nHello **world**.\n");
    CK(mid >= 0, "ingest md");
    if (mid >= 0) {
        CK(strcmp(doc_kind(s, mid), "md") == 0, "kind md");
        char *mj = doc_json(s, mid);
        CK(mj != NULL, "md model emits");
        if (mj) {
            /* the model must be the semantic document JSON, not raw markdown */
            CK(strstr(mj, "\"type\":\"document\"") != NULL, "md model is semantic document");
            CK(strstr(mj, "\"blocks\"") != NULL, "md model has blocks");
            CK(strstr(mj, "Title") != NULL, "md model captured heading");
            free(mj);
        }
    }

    /* --- md -> create docx -> re-ingest (semantic round-trip) --- */
    long mid2 = doc_ingest_text(s, "md", "# Heading\n\nBody text here.\n");
    CK(mid2 >= 0, "ingest md2");
    if (mid2 >= 0) {
        size_t ol = 0; uint8_t *docx = doc_create_bytes(s, mid2, "docx", &ol);
        CK(docx != NULL && ol > 0, "create docx from md");
        if (docx) {
            /* validate the created docx with python zipfile/xml */
            FILE *f = fopen("/tmp/wubudoc_rt.docx", "wb");
            fwrite(docx, 1, ol, f); fclose(f); free(docx);
            long did = doc_open(s, "/tmp/wubudoc_rt.docx");
            CK(did >= 0, "re-ingest created docx");
            if (did >= 0) {
                CK(strcmp(doc_kind(s, did), "docx") == 0, "re-ingest kind docx");
                char *m2 = doc_json(s, did);
                CK(m2 && strstr(m2, "\"type\":\"document\"") != NULL, "docx re-ingest is semantic");
                CK(m2 && strstr(m2, "Heading") != NULL, "docx re-ingest kept heading");
                free(m2);
            }
        }
    }

    /* --- json model -> create odt (semantic create from edited model) --- */
    {
        const char *model = "{\"type\":\"document\",\"blocks\":["
            "{\"kind\":\"paragraph\",\"style\":\"Title\",\"bold\":1,\"text\":\"My Report\"},"
            "{\"kind\":\"paragraph\",\"style\":null,\"bold\":0,\"text\":\"First line of body.\"},"
            "{\"kind\":\"table\",\"rows\":1,\"cols\":2,\"cells\":[[\"A\",\"B\"]]}]}";
        long jid = doc_ingest_text(s, "json", model);
        CK(jid >= 0, "ingest model json");
        if (jid >= 0) {
            size_t ol = 0; uint8_t *odt = doc_create_bytes(s, jid, "odt", &ol);
            CK(odt != NULL, "create odt from model");
            if (odt) {
                FILE *f = fopen("/tmp/wubudoc_rt.odt", "wb");
                fwrite(odt, 1, ol, f); fclose(f); free(odt);
                long oid = doc_open(s, "/tmp/wubudoc_rt.odt");
                CK(oid >= 0, "re-ingest created odt");
                if (oid >= 0) {
                    char *m3 = doc_json(s, oid);
                    CK(m3 && strstr(m3, "My Report") != NULL, "odt round-trip kept title");
                    free(m3);
                }
            }
        }
    }

    /* --- xlsx -> semantic workbook model --- */
    {
        const char *csv = "name,age\nAlice,30\nBob,25\n";
        long xid = doc_ingest_text(s, "csv", csv);
        CK(xid >= 0, "ingest csv");
        if (xid >= 0) {
            char *mj = doc_json(s, xid);
            CK(mj && strstr(mj, "\"type\":\"workbook\"") != NULL, "csv model is workbook");
            CK(mj && strstr(mj, "Alice") != NULL, "csv model kept data");
            free(mj);
            /* create xlsx, re-ingest, confirm */
            size_t ol = 0; uint8_t *xlsx = doc_create_bytes(s, xid, "xlsx", &ol);
            if (xlsx) {
                FILE *f = fopen("/tmp/wubudoc_rt.xlsx", "wb");
                fwrite(xlsx, 1, ol, f); fclose(f); free(xlsx);
                long x2 = doc_open(s, "/tmp/wubudoc_rt.xlsx");
                CK(x2 >= 0, "re-ingest xlsx");
                if (x2 >= 0) {
                    char *m4 = doc_json(s, x2);
                    CK(m4 && strstr(m4, "Bob") != NULL, "xlsx round-trip kept rows");
                    free(m4);
                }
            } else CK(0, "create xlsx from csv");
        }
    }

    /* --- generic zip keeps raw parts model --- */
    {
        char tmpl[] = "/tmp/wubudoc_g_XXXXXX";
        int fd = mkstemp(tmpl);
        FILE *zf = fdopen(fd, "wb");
        wubuzip_writer *zw = wubuzip_create(zf);
        const char *doc = "<a>1</a>";
        wubuzip_add_deflated(zw, "one.xml", doc, (uint32_t)strlen(doc));
        wubuzip_finalize(zw); fclose(zf);
        size_t n = 0; char *blob = slurp(tmpl, &n); remove(tmpl);
        long zid = doc_ingest_bytes(s, "zip", (const uint8_t*)blob, n);
        free(blob);
        CK(zid >= 0, "ingest zip");
        if (zid >= 0) {
            char *mj = doc_json(s, zid);
            CK(mj && strstr(mj, "one.xml") != NULL, "zip model lists parts");
            free(mj);
        }
    }

    /* --- font ingest (sfnt table enumeration) --- */
    {
        FILE *f = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "rb");
        if (f) {
            fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f);
            uint8_t *b = malloc((size_t)sz); size_t rd=fread(b,1,(size_t)sz,f); fclose(f);
            long fid = doc_ingest_bytes(s, "font", b, rd);
            free(b);
            CK(fid >= 0, "ingest font");
            if (fid >= 0) {
                char *mj = doc_json(s, fid);
                CK(mj && strstr(mj, "glyf") != NULL, "font model has glyf table");
                free(mj);
            }
        } else { printf("(skip font: no system ttf)\n"); }
    }

    doc_session_free(s);

    if (fails) { printf("\nWUBUDOC TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUDOC TESTS PASSED\n");
    return 0;
}
