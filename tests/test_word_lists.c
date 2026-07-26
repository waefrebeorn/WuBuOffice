/* test_word_lists.c -- prove the WuBuWord list feature (bullet + numbered)
 * produces a valid .docx: word/numbering.xml is emitted, the document
 * references it, and the package re-reads cleanly. Self-contained, no
 * external oracle. C11. */
#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../src/wubuoxml/reader.h"
#include "../src/wubuzip/reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int contains(const uint8_t *b, size_t n, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0 || nl > n) return 0;
    for (size_t i = 0; i + nl <= n; i++)
        if (memcmp(b + i, needle, nl) == 0) return 1;
    return 0;
}

int main(void) {
    int fail = 0;

    wubuword_doc *d = wubuword_create();
    if (!d) { printf("FAIL create\n"); return 1; }

    wubuword_para(d, "Title", 0, "List Demo");
    wubuword_para(d, "Heading1", 0, "Shopping");

    wubuword_list_begin(d, "bullet");
    wubuword_list_item(d, "Milk");
    wubuword_list_item(d, "Eggs");
    wubuword_list_item(d, "Bread");
    wubuword_list_end(d);

    wubuword_para(d, NULL, 0, "Steps:");
    wubuword_list_begin(d, "number");
    wubuword_list_item(d, "Wake up");
    wubuword_list_item(d, "Coffee");
    wubuword_list_item(d, "Code");
    wubuword_list_end(d);

    if (!wubuword_used_lists(d)) { printf("FAIL used_lists not set\n"); fail++; }

    const char *out = "/tmp/wubuword_lists.docx";
    if (wubuword_assemble_doc(out, d) != 0) { printf("FAIL assemble\n"); fail++; }
    wubuword_free(d);

    /* read the package back */
    FILE *f = fopen(out, "rb");
    if (!f) { printf("FAIL open result\n"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { printf("FAIL read\n"); return 1; }
    fclose(f);

    wubuoxml_package pkg;
    if (wubuoxml_read(buf, (size_t)n, &pkg) != 0) { printf("FAIL ooxml read\n"); fail++; }

    const wubuoxml_part *num = wubuoxml_part_find(&pkg, "word/numbering.xml");
    if (!num) { printf("FAIL no numbering.xml part\n"); fail++; }
    else {
        if (!contains(num->bytes, num->len, "<w:num w:numId=\"1\""))
            { printf("FAIL numbering missing bullet numId=1\n"); fail++; }
        if (!contains(num->bytes, num->len, "<w:num w:numId=\"2\""))
            { printf("FAIL numbering missing number numId=2\n"); fail++; }
    }

    /* [Content_Types].xml must declare the numbering override */
    const wubuoxml_part *ct = wubuoxml_part_find(&pkg, "[Content_Types].xml");
    if (ct && !contains(ct->bytes, ct->len, "numbering.xml"))
        { printf("FAIL Content_Types missing numbering override\n"); fail++; }
    else if (!ct) { printf("FAIL no [Content_Types].xml\n"); fail++; }

    const wubuoxml_part *docp = wubuoxml_part_find(&pkg, "word/document.xml");
    if (!docp) { printf("FAIL no document.xml\n"); fail++; }
    else {
        if (!contains(docp->bytes, docp->len, "w:numPr"))
            { printf("FAIL document has no w:numPr\n"); fail++; }
        if (!contains(docp->bytes, docp->len, "Milk"))
            { printf("FAIL document missing list item text\n"); fail++; }
    }

    /* document.xml must relate to numbering.xml */
    if (docp && num) {
        int related = 0;
        for (size_t i = 0; i < docp->nrel; i++)
            if (docp->rel_targets[i] && strcmp(docp->rel_targets[i], "word/numbering.xml") == 0)
                related = 1;
        if (!related) { printf("FAIL document.xml does not relate to numbering.xml\n"); fail++; }
    }

    wubuoxml_free(&pkg);
    free(buf);

    if (fail) { printf("test_word_lists: FAIL (%d)\n", fail); return 1; }
    printf("test_word_lists: PASS\n");
    return 0;
}
