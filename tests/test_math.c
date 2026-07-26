/* test_math.c -- wubumath acceptance test (math expression -> SVG). */
#include "math.h"
#include "wubusvg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

static int well_formed(const char *svg){
    SvgDoc *d = svg_parse(svg, strlen(svg));
    if (!d) return 0;
    SvgNode *r = svg_root(d);
    int ok = r && strcmp(svg_node_name(r), "svg") == 0;
    svg_free(d);
    return ok;
}

int main(void){
    /* plain identifier + operator */
    char *svg = math_render_svg("x + 1");
    CHECK(svg != NULL, "x+1 rendered");
    if (svg){
        CHECK(well_formed(svg), "x+1 well-formed");
        CHECK(strstr(svg, "<text") != NULL, "x+1 has text");
        free(svg);
    }

    /* fraction */
    svg = math_render_svg("\\frac{a}{b}");
    CHECK(svg != NULL, "frac rendered");
    if (svg){
        CHECK(well_formed(svg), "frac well-formed");
        CHECK(strstr(svg, "<line") != NULL, "frac has fraction rule");
        CHECK(strstr(svg, ">a<") != NULL, "frac has numerator a");
        CHECK(strstr(svg, ">b<") != NULL, "frac has denominator b");
        free(svg);
    }

    /* superscript */
    svg = math_render_svg("x^2");
    CHECK(svg != NULL, "x^2 rendered");
    if (svg){
        CHECK(well_formed(svg), "x^2 well-formed");
        CHECK(strstr(svg, ">2<") != NULL, "x^2 has superscript 2");
        free(svg);
    }

    /* subscript */
    svg = math_render_svg("x_0");
    CHECK(svg != NULL, "x_0 rendered");
    if (svg){
        CHECK(well_formed(svg), "x_0 well-formed");
        CHECK(strstr(svg, ">0<") != NULL, "x_0 has subscript 0");
        free(svg);
    }

    /* parentheses group + sup */
    svg = math_render_svg("(a+b)^2");
    CHECK(svg != NULL, "(a+b)^2 rendered");
    if (svg){
        CHECK(well_formed(svg), "(a+b)^2 well-formed");
        CHECK(strstr(svg, ">2<") != NULL, "(a+b)^2 has sup 2");
        free(svg);
    }

    /* NULL / empty */
    CHECK(math_render_svg(NULL) == NULL, "NULL -> NULL");
    CHECK(math_render_svg("") == NULL, "empty -> NULL");

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubumath (frac/sup/sub/group, wubusvg-validated)\n");
    return 0;
}
