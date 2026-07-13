/* test_wubuxml_parser.c — regression guard for ws05#0884 parser.
 *
 * The parser MUST emit element-END events in true document order,
 * immediately when it reads a </name> tag. An earlier build deferred
 * every close to a single LIFO drain at EOF, which scrambles the event
 * stream for any document with sibling or nested elements (e.g. a
 * .docx with two paragraphs and a table) and makes children land under
 * the wrong parent. This test asserts the event order directly.
 *
 * CHECK (not assert) so failures surface under -DNDEBUG. */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

/* Event recorder: a linear log of (evt, name) pairs.
 * The parser owns `info->name` only for the duration of the callback,
 * so we strdup it into our own storage. */
#define MAX_EVT 256
#define MAX_NLEN 64
static int g_kind[MAX_EVT];
static char g_name[MAX_EVT][MAX_NLEN];
static int g_n;

static int record(wubuxml_event evt, const wubuxml_info *info, void *u) {
    (void)u;
    if (g_n < MAX_EVT) {
        g_kind[g_n] = (int)evt;
        const char *src = (evt == WUBUXML_EVT_TEXT) ? "[text]"
                                                    : (info->name ? info->name : "");
        strncpy(g_name[g_n], src, MAX_NLEN - 1);
        g_name[g_n][MAX_NLEN - 1] = 0;
        g_n++;
    }
    return 0;
}

/* Assert the recorded events equal an expected sequence.
 * Spec: "START:a" "END:a" "TEXT" etc. (END = element close). For
 * START/END the next token is the element name; TEXT carries no name. */
static int expect(const char *spec) {
    int want[MAX_EVT], nwant = 0;
    const char *names[MAX_EVT];
    char buf[64];
    const char *p = spec;
    while (*p) {
        size_t i = 0;
        while (*p && *p != ' ') buf[i++] = *p++;
        buf[i] = 0;
        if (*p == ' ') p++;
        int is_text = 0;
        if (strcmp(buf, "START") == 0)      want[nwant] = WUBUXML_EVT_START;
        else if (strcmp(buf, "END") == 0)   want[nwant] = WUBUXML_EVT_END;
        else if (strcmp(buf, "TEXT") == 0)  { want[nwant] = WUBUXML_EVT_TEXT; is_text = 1; }
        else { fprintf(stderr, "bad spec token %s\n", buf); return 1; }
        if (!is_text) {
            i = 0;
            while (*p && *p != ' ') buf[i++] = *p++;
            buf[i] = 0;
            if (*p == ' ') p++;
            names[nwant] = (*buf && strcmp(buf, "-") != 0) ? strdup(buf) : NULL;
        } else {
            names[nwant] = NULL;
        }
        nwant++;
    }
    CHECK(g_n == nwant, "event count");
    for (int i = 0; i < nwant; i++) {
        CHECK(g_kind[i] == want[i], "event kind order");
        if (names[i]) {
            CHECK(g_name[i] && strcmp(g_name[i], names[i]) == 0, "event name");
        }
    }
    return 0;
}

static int run(const char *xml) {
    g_n = 0;
    int rc = wubuxml_parse((const uint8_t *)xml, strlen(xml), record, NULL);
    CHECK(rc == 0, "parse rc");
    return rc != 0 ? 1 : 0;
}

int main(void) {
    /* 1) two SIBLING elements -> START/END must NOT be deferred. */
    if (run("<root><a>1</a><a>2</a></root>")) return 1;
    if (expect("START root START a TEXT END a START a TEXT END a END root")) return 1;

    /* 2) NESTED elements -> inner END before outer END. */
    if (run("<a><b><c>x</c></b></a>")) return 1;
    if (expect("START a START b START c TEXT END c END b END a")) return 1;

    /* 3) self-closing + text. */
    if (run("<a>x<empty/></a>")) return 1;
    if (expect("START a TEXT START empty END empty END a")) return 1;

    /* 4) attribute + entity decode on TEXT. */
    if (run("<p id=\"x\">&amp;&lt;</p>")) return 1;
    if (expect("START p TEXT END p")) return 1;

    /* 5) the exact shape that exposed the bug: 2 paragraphs + table. */
    if (run("<w:body><w:p><w:r><w:t>one</w:t></w:r></w:p>"
                   "<w:p><w:r><w:t>two</w:t></w:r></w:p>"
                   "<w:tbl><w:tr><w:tc><w:p><w:r><w:t>c</w:t></w:r></w:p></w:tc></w:tr></w:tbl>"
            "</w:body>")) return 1;
    if (expect("START w:body START w:p START w:r START w:t TEXT END w:t END w:r END w:p"
               " START w:p START w:r START w:t TEXT END w:t END w:r END w:p"
               " START w:tbl START w:tr START w:tc START w:p START w:r START w:t TEXT END w:t END w:r END w:p END w:tc END w:tr END w:tbl"
               " END w:body")) return 1;

    /* 6) MALFORMED input MUST be rejected (rc != 0), not silently parsed.
     * A deferred/misordered close is exactly how the ws05#0884 load-drop
     * bug stayed hidden, so the parser must fail closed on bad structure. */
    if (wubuxml_parse((const uint8_t *)"<a><b></a></b>", 16, record, NULL) == 0) {
        fprintf(stderr, "FAIL %s:%d mismatched close accepted\n", __FILE__, __LINE__);
        return 1;
    }
    if (wubuxml_parse((const uint8_t *)"<a><b><c>", 9, record, NULL) == 0) {
        fprintf(stderr, "FAIL %s:%d unclosed tags accepted\n", __FILE__, __LINE__);
        return 1;
    }
    if (wubuxml_parse((const uint8_t *)"<a></b>", 7, record, NULL) == 0) {
        fprintf(stderr, "FAIL %s:%d close-without-open accepted\n", __FILE__, __LINE__);
        return 1;
    }
    if (wubuxml_parse((const uint8_t *)"</a>", 4, record, NULL) == 0) {
        fprintf(stderr, "FAIL %s:%d bare close accepted\n", __FILE__, __LINE__);
        return 1;
    }

    printf("wubuxml_parser: closing-tag event order + nesting + entities + malformed-rejection PASSED "
           "(ws05#0884 regression)\n");
    return 0;
}
