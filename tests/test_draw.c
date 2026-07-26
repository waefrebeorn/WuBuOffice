/* test_draw.c -- wubudraw acceptance test (vector shapes -> SVG). */
#include "draw.h"
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
    DrawScene *s = draw_create(200, 100);
    CHECK(s != NULL, "scene created");

    draw_add_rect(s, 10, 10, 80, 40, "#4e79a7", "#222");
    draw_add_ellipse(s, 150, 50, 30, 20, "#59a14f", "#222");
    draw_add_line(s, 0, 90, 200, 90, "#e15759", 2);
    double pts[] = {5,5, 50,5, 50,50, 5,50};
    draw_add_polyline(s, pts, 4, "#000", 1, 1);
    draw_add_text(s, 10, 80, "Hi & <there>", 16, "#000");

    char *svg = draw_render_svg(s);
    CHECK(svg != NULL, "draw svg rendered");
    if (svg){
        CHECK(well_formed(svg), "draw svg well-formed via wubusvg");
        CHECK(strstr(svg, "<rect") != NULL, "has rect");
        CHECK(strstr(svg, "<ellipse") != NULL, "has ellipse");
        CHECK(strstr(svg, "<line") != NULL, "has line");
        CHECK(strstr(svg, "<polyline") != NULL, "has polyline");
        CHECK(strstr(svg, "<text") != NULL, "has text");
        CHECK(strstr(svg, "&amp;") != NULL && strstr(svg, "&lt;"), "text XML-escaped");
        free(svg);
    }
    draw_destroy(s);

    /* NULL guards */
    CHECK(draw_render_svg(NULL) == NULL, "NULL scene -> NULL svg");

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubudraw (rect/ellipse/line/polyline/text, wubusvg-validated)\n");
    return 0;
}
