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

    /* ---------- query + edit-by-query (agent targeting) ---------- */
    {
        SvgDoc *d = svg_parse(SRC, strlen(SRC));
        CK(d != NULL, "query: parse base doc");
        SvgNode *r = svg_root(d);

        /* svg_find: root echo ignored */
        SvgNode *g = svg_find(r, "svg/g");
        CK(g != NULL && strcmp(svg_node_name(g), "g") == 0, "svg_find 'svg/g' finds <g>");
        SvgNode *rect = svg_find(r, "g/rect");
        CK(rect != NULL && strcmp(svg_node_name(rect), "rect") == 0, "svg_find 'g/rect' finds <rect>");

        /* svg_find_all: collect glyphs */
        SvgNode *gs[16];
        size_t gn = svg_find_all(r, "glyph", gs, 16);
        CK(gn == 2, "svg_find_all 'glyph' -> 2");

        /* edit-by-query: set an attr on the rect via path */
        CK(svg_set_attr_path(r, "g/rect", "class", "queried") == 0, "set-attr-by-path");
        CK(strcmp(svg_attr(rect, "class"), "queried") == 0, "attr set on found rect");

        /* edit-by-query: remove the <text> under <g> */
        size_t kids_before = svg_child_count(g);
        int rc = svg_remove_path(r, "g/text");
        CK(rc == 1, "remove-by-path returns 1");
        CK(svg_child_count(g) == kids_before - 1, "text removed from <g>");
        CK(svg_find(r, "g/text") == NULL, "text gone after remove-by-path");

        /* no-match returns -1, not crash */
        CK(svg_set_attr_path(r, "g/nonexistent", "k", "v") == -1, "set-attr no-match -> -1");
        CK(svg_remove_path(r, "nonexistent") == -1, "remove no-match -> -1");

        char *q = svg_regurgitate(d);
        CK(q != NULL, "regurgitate queried doc");
        if (q) {
            CK(strstr(q, "class=\"queried\"") != NULL, "queried attr in output");
            CK(strstr(q, "<text") == NULL, "removed text absent in output");
            FILE *tf = fopen("/tmp/wubusvg_queried.svg", "wb");
            if (tf) { fputs(q, tf); fclose(tf); }
            SvgDoc *d2 = svg_parse(q, strlen(q));
            CK(d2 != NULL, "re-ingest queried");
            if (d2) {
                SvgNode *r2 = svg_find(svg_root(d2), "g/rect");
                CK(r2 && strcmp(svg_attr(r2, "class"), "queried") == 0, "query edit persisted");
                svg_free(d2);
            }
            free(q);
        }
        svg_free(d);
    }

    /* ---------- editing (creation half) ---------- */
    {
        SvgDoc *d = svg_parse(SRC, strlen(SRC));
        CK(d != NULL, "edit: parse base doc");
        SvgNode *r = svg_root(d);

        /* set/overwrite + remove attribute */
        CK(svg_set_attr(r, "width", "200") == 0, "set existing attr");
        CK(strcmp(svg_attr(r, "width"), "200") == 0, "attr overwritten");
        CK(svg_set_attr(r, "data-agi", "wubuos") == 0, "set new attr");
        CK(strcmp(svg_attr(r, "data-agi"), "wubuos") == 0, "new attr present");
        CK(svg_remove_attr(r, "height") == 1, "remove existing attr");
        CK(svg_attr(r, "height") == NULL, "removed attr gone");
        CK(svg_remove_attr(r, "nope") == 0, "remove absent attr is 0");

        /* create + append a new <rect>, then insert one at front of <g> */
        SvgNode *g = NULL;
        for (size_t i = 0; i < svg_child_count(r); i++)
            if (strcmp(svg_node_name(svg_child(r, i)), "g") == 0) g = svg_child(r, i);
        CK(g != NULL, "found <g>");
        size_t g_before = svg_child_count(g);

        SvgNode *nr = svg_new_node("rect");
        svg_set_attr(nr, "x", "9"); svg_set_attr(nr, "class", "added");
        CK(svg_append_child(g, nr) == 0, "append new <rect>");
        CK(svg_child_count(g) == g_before + 1, "child count grew");

        SvgNode *circ = svg_new_node("circle");
        svg_set_attr(circ, "r", "5");
        CK(svg_insert_child(g, 0, circ) == 0, "insert <circle> at front");
        CK(strcmp(svg_node_name(svg_child(g, 0)), "circle") == 0, "circle is first child");
        CK(svg_count_tag(r, "circle") == 1, "one circle in tree");
        CK(svg_count_tag(r, "rect") == 2, "two rects now");

        /* set text on a fresh node */
        SvgNode *lbl = svg_new_node("text");
        svg_set_text(lbl, "created by <wubuOS> & agent");
        CK(svg_append_child(g, lbl) == 0, "append text node");

        /* remove the first child (circle) */
        CK(svg_remove_child(g, 0) == 1, "remove first child");
        CK(svg_count_tag(r, "circle") == 0, "circle removed");

        /* regurgitate the EDITED doc; must be well-formed + reflect edits */
        char *eout = svg_regurgitate(d);
        CK(eout != NULL, "regurgitate edited doc");
        if (eout) {
            CK(strstr(eout, "width=\"200\"") != NULL, "edit: width=200 in output");
            CK(strstr(eout, "data-agi=\"wubuos\"") != NULL, "edit: new attr in output");
            CK(strstr(eout, "&lt;wubuOS&gt; &amp; agent") != NULL, "edit: text properly escaped");
            FILE *tf = fopen("/tmp/wubusvg_edited.svg", "wb");
            if (tf) { fputs(eout, tf); fclose(tf); }
            /* re-ingest the edited output: proves edits survive round-trip */
            SvgDoc *d2 = svg_parse(eout, strlen(eout));
            CK(d2 != NULL, "re-ingest edited output");
            if (d2) {
                SvgNode *r2 = svg_root(d2);
                CK(strcmp(svg_attr(r2, "width"), "200") == 0, "edit persisted through round-trip");
                CK(svg_attr(r2, "height") == NULL, "edit: removed root attr stays gone");
                CK(strcmp(svg_attr(r2, "data-agi"), "wubuos") == 0, "edit: new attr persisted");
                svg_free(d2);
            }
            free(eout);
        }
        svg_free(d);
    }

    /* malformed input must be rejected, not crash */
    SvgDoc *bad = svg_parse("<svg><g></svg>", 14);
    CK(bad == NULL, "unbalanced tags rejected");
    svg_free(bad);

    if (fails) { printf("\nWUBUSVG TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUSVG TESTS PASSED\n");
    return 0;
}
