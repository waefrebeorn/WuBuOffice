/* test_wubuview.c -- docflat model->text flattening (pure, no TTY). */
#include "docflat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

int main(void) {
    /* document model: blocks -> paragraphs */
    {
        const char *m = "{\"type\":\"document\",\"blocks\":["
                        "{\"kind\":\"heading\",\"text\":\"Title\"},"
                        "{\"kind\":\"paragraph\",\"text\":\"Hello world.\"}]}";
        char *t = docflat_from_json(m);
        CK(t != NULL, "flatten doc model");
        CK(strstr(t, "Title") != NULL, "keeps heading text");
        CK(strstr(t, "====") != NULL, "heading underlined");
        CK(strstr(t, "Hello world.") != NULL, "keeps paragraph text");
        /* heading appears before paragraph */
        CK(strstr(t, "Title") < strstr(t, "Hello world."), "reading order preserved");
        free(t);
    }

    /* doc_json() wraps the model under "model": unwrap it */
    {
        const char *m = "{\"model\":{\"type\":\"document\",\"blocks\":["
                        "{\"kind\":\"heading\",\"text\":\"Wrapped\"},"
                        "{\"kind\":\"paragraph\",\"text\":\"Body text here.\"}]}}";
        char *t = docflat_from_json(m);
        CK(t != NULL, "flatten model-wrapped doc");
        CK(strstr(t, "Wrapped") != NULL, "unwraps model key: heading");
        CK(strstr(t, "Body text here.") != NULL, "unwraps model key: body");
        CK(strstr(t, "\"model\"") == NULL, "no raw JSON leak (real flatten)");
        free(t);
    }

    /* heading detected by style field (md/docx use style="Heading1") */
    {
        const char *m = "{\"model\":{\"type\":\"document\",\"blocks\":["
                        "{\"kind\":\"paragraph\",\"style\":\"Heading1\",\"text\":\"Styled Head\"}]}}";
        char *t = docflat_from_json(m);
        CK(t && strstr(t, "Styled Head") != NULL, "style-heading text kept");
        CK(t && strstr(t, "====") != NULL, "style-heading underlined");
        free(t);
    }

    /* text wrapper */
    {
        char *t = docflat_from_json("{\"text\":\"just some text\"}");
        CK(t && strstr(t, "just some text") != NULL, "flatten text wrapper");
        free(t);
    }

    /* sheet model: rows -> tab-separated */
    {
        const char *m = "{\"rows\":[[\"a\",\"b\"],[1,2]]}";
        char *t = docflat_from_json(m);
        CK(t != NULL, "flatten sheet model");
        CK(strstr(t, "a\tb") != NULL, "row0 tab-separated");
        CK(strstr(t, "1\t2") != NULL, "row1 numbers");
        free(t);
    }

    /* sheet cells as objects */
    {
        const char *m = "{\"rows\":[[{\"text\":\"x\"},{\"v\":42}]]}";
        char *t = docflat_from_json(m);
        CK(t && strstr(t, "x\t42") != NULL, "object cells flattened");
        free(t);
    }

    /* unrecognized object -> falls back to JSON (nothing hidden) */
    {
        char *t = docflat_from_json("{\"weird\":123}");
        CK(t && strstr(t, "weird") != NULL, "unknown model falls back to JSON");
        free(t);
    }

    /* non-JSON input -> echoed raw */
    {
        char *t = docflat_from_json("plain text not json");
        CK(t && strstr(t, "plain text not json") != NULL, "non-JSON echoed");
        free(t);
    }

    /* NULL safety */
    CK(docflat_from_json(NULL) == NULL, "NULL input -> NULL");

    if (fails) { printf("WUBUVIEW TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUVIEW TESTS PASSED\n");
    return 0;
}
