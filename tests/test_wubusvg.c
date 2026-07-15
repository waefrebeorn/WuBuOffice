/* test_wubusvg.c -- SVG ingest + regurgitate round-trip.
 * Builds a document from a known SVG, asserts the structural model, then
 * regurgitates + re-ingests and asserts the tree is structurally identical
 * (idempotent). The emitted SVG is also written to /tmp for an INDEPENDENT
 * XML well-formedness check by the ctest driver. */
#include "wubusvg.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

static const char *SRC =
    "<?xml version=\"1.0\"?>\n"
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">\n"
    "  <defs>\n"
    "    <font id=\"f1\">\n"
    "      <font-face font-family=\"Test\" units-per-em=\"1000\"/>\n"
    "      <glyph unicode=\"A\" d=\"M0 0L10 10Z\"/>\n"
    "      <glyph unicode=\"B\" d=\"M0 0L20 20Z\"/>\n"
    "    </font>\n"
    "  </defs>\n"
    "  <g id=\"layer1\">\n"
    "    <rect x=\"1\" y=\"2\" width=\"3\" height=\"4\"/>\n"
    "    <text x=\"5\" y=\"6\">Hello &amp; welcome</text>\n"
    "  </g>\n"
    "</svg>\n";

/* Structural fingerprint: name + attr_count + child_count recursively. */
static void fingerprint(const SvgNode *n, char *out, size_t cap, size_t *pos) {
    if (!n || *pos + 64 >= cap) return;
    *pos += (size_t)snprintf(out + *pos, cap - *pos, "(%s#%zu.%zu",
                             svg_node_name(n), svg_attr_count(n), svg_child_count(n));
    for (size_t i = 0; i < svg_child_count(n); i++)
        fingerprint(svg_child(n, i), out, cap, pos);
    if (*pos + 2 < cap) { out[(*pos)++] = ')'; out[*pos] = '\0'; }
}

int main(void) {
    SvgDoc *doc = svg_parse(SRC, strlen(SRC));
    CK(doc != NULL, "svg_parse succeeds on well-formed SVG");
    if (!doc) { printf("\nWUBUSVG TESTS FAILED (%d)\n", fails); return 1; }

    SvgNode *root = svg_root(doc);
    CK(root != NULL, "root exists");
    CK(strcmp(svg_node_name(root), "svg") == 0, "root is <svg>");
    CK(svg_attr(root, "width") && strcmp(svg_attr(root, "width"), "100") == 0, "root width=100");
    CK(svg_attr(root, "viewBox") && strcmp(svg_attr(root, "viewBox"), "0 0 100 50") == 0, "viewBox preserved");
    CK(svg_attr(root, "nope") == NULL, "absent attr returns NULL");

    /* subtree tag counting */
    CK(svg_count_tag(root, "glyph") == 2, "two <glyph> in subtree");
    CK(svg_count_tag(root, "font-face") == 1, "one <font-face>");
    CK(svg_count_tag(root, "rect") == 1, "one <rect>");
    CK(svg_count_tag(root, "svg") == 1, "one <svg> (self)");

    /* text content + entity decode */
    /* find the <text> node: svg > g > text */
    SvgNode *g = NULL;
    for (size_t i = 0; i < svg_child_count(root); i++)
        if (strcmp(svg_node_name(svg_child(root, i)), "g") == 0) g = svg_child(root, i);
    CK(g != NULL, "found <g> layer");
    if (g) {
        SvgNode *txt = NULL;
        for (size_t i = 0; i < svg_child_count(g); i++)
            if (strcmp(svg_node_name(svg_child(g, i)), "text") == 0) txt = svg_child(g, i);
        CK(txt != NULL, "found <text>");
        if (txt) CK(strstr(svg_node_text(txt), "Hello & welcome") != NULL,
                    "text entity &amp; decoded to &");
    }

    /* regurgitate */
    char *out = svg_regurgitate(doc);
    CK(out != NULL, "regurgitate returns");
    if (out) {
        CK(strstr(out, "<svg") != NULL, "output has <svg>");
        CK(strstr(out, "<glyph") != NULL, "output has <glyph>");
        CK(strstr(out, "&amp;") != NULL, "output re-escapes & as &amp;");
        FILE *tf = fopen("/tmp/wubusvg_out.svg", "wb");
        if (tf) { fputs(out, tf); fclose(tf); }

        /* idempotent: re-ingest the regurgitated output; fingerprints match */
        SvgDoc *doc2 = svg_parse(out, strlen(out));
        CK(doc2 != NULL, "re-ingest regurgitated output");
        if (doc2) {
            char f1[4096] = {0}, f2[4096] = {0};
            size_t p1 = 0, p2 = 0;
            fingerprint(root, f1, sizeof f1, &p1);
            fingerprint(svg_root(doc2), f2, sizeof f2, &p2);
            CK(strcmp(f1, f2) == 0, "round-trip preserves structure (idempotent)");
            svg_free(doc2);
        }
        free(out);
    }

    svg_free(doc);

    /* malformed input must be rejected, not crash */
    SvgDoc *bad = svg_parse("<svg><g></svg>", 14);
    CK(bad == NULL, "unbalanced tags rejected");
    svg_free(bad);

    if (fails) { printf("\nWUBUSVG TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUSVG TESTS PASSED\n");
    return 0;
}
