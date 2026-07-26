/* test_chart.c -- wubuchart acceptance test (SVG chart rendering). */
#include "chart.h"
#include "wubusvg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

/* re-parse the produced SVG to confirm wubusvg accepts it (well-formed) */
static int is_well_formed_svg(const char *svg){
    SvgDoc *d = svg_parse(svg, strlen(svg));
    if (!d) return 0;
    SvgNode *root = svg_root(d);
    int ok = root && strcmp(svg_node_name(root), "svg") == 0;
    svg_free(d);
    return ok;
}

static int count_substr(const char *s, const char *sub){
    int n=0; const char *p=s;
    while ((p=strstr(p, sub))) { n++; p += strlen(sub); }
    return n;
}

int main(void){
    /* --- BAR --- */
    double sales[] = {10, 25, 15, 30};
    const char *lbl[] = {"Q1","Q2","Q3","Q4"};
    Chart *c = chart_create("Quarterly Sales");
    chart_add_series(c, "units", sales, 4, lbl);
    char *svg = chart_render_svg(c);
    CHECK(svg != NULL, "bar svg rendered");
    if (svg){
        CHECK(is_well_formed_svg(svg), "bar svg well-formed via wubusvg");
        CHECK(strstr(svg, "<rect") != NULL, "bar has rects");
        CHECK(count_substr(svg, "<rect") >= 4, "bar has >=4 rects");
        CHECK(strstr(svg, "Quarterly Sales") != NULL, "bar has title");
        CHECK(strstr(svg, "Q2") != NULL, "bar has x label Q2");
        free(svg);
    }
    chart_free(c);

    /* --- LINE --- */
    double t[] = {3, 7, 5, 9, 6};
    const char *tl[] = {"Mon","Tue","Wed","Thu","Fri"};
    c = chart_create("Trend");
    chart_set_type(c, CHART_LINE);
    chart_add_series(c, "val", t, 5, tl);
    svg = chart_render_svg(c);
    CHECK(svg != NULL, "line svg rendered");
    if (svg){
        CHECK(is_well_formed_svg(svg), "line svg well-formed");
        CHECK(strstr(svg, "<path") != NULL, "line has path");
        CHECK(strstr(svg, "<circle") != NULL, "line has markers");
        free(svg);
    }
    chart_free(c);

    /* --- PIE --- */
    double parts[] = {40, 30, 20, 10};
    const char *pl[] = {"A","B","C","D"};
    c = chart_create("Share");
    chart_set_type(c, CHART_PIE);
    chart_add_series(c, "share", parts, 4, pl);
    svg = chart_render_svg(c);
    CHECK(svg != NULL, "pie svg rendered");
    if (svg){
        CHECK(is_well_formed_svg(svg), "pie svg well-formed");
        CHECK(strstr(svg, "<path") != NULL, "pie has wedges");
        CHECK(strstr(svg, "A") != NULL && strstr(svg, "D") != NULL, "pie legend labels");
        free(svg);
    }
    chart_free(c);

    /* --- SCATTER --- */
    double pts[] = {1.0, 2.5, 1.8, 3.2, 2.1};
    c = chart_create("Scatter");
    chart_set_type(c, CHART_SCATTER);
    chart_add_series(c, "pts", pts, 5, NULL);
    svg = chart_render_svg(c);
    CHECK(svg != NULL, "scatter svg rendered");
    if (svg){
        CHECK(is_well_formed_svg(svg), "scatter svg well-formed");
        CHECK(count_substr(svg, "<circle") >= 5, "scatter has >=5 circles");
        free(svg);
    }
    chart_free(c);

    /* --- FORMULA subtitle --- */
    c = chart_create("With Formula");
    chart_add_series(c, "x", sales, 4, lbl);
    chart_set_subtitle_formula(c, "SUM(10,25,15,30)");
    svg = chart_render_svg(c);
    CHECK(svg != NULL, "formula chart rendered");
    if (svg){
        CHECK(is_well_formed_svg(svg), "formula chart well-formed");
        CHECK(strstr(svg, "80") != NULL, "formula subtitle shows 80 (10+25+15+30)");
        free(svg);
    }
    chart_free(c);

    /* --- NO series -> NULL --- */
    c = chart_create("Empty");
    svg = chart_render_svg(c);
    CHECK(svg == NULL, "no-series chart returns NULL");
    chart_free(c);

    /* --- dump a sample bar chart to disk when CHART_DUMP is set --- */
    if (getenv("CHART_DUMP")) {
        double sy[] = {10, 25, 15, 30}; const char *sl[] = {"Q1","Q2","Q3","Q4"};
        Chart *dc = chart_create("Quarterly Sales");
        chart_add_series(dc, "units", sy, 4, sl);
        char *dsvg = chart_render_svg(dc);
        if (dsvg) {
            FILE *df = fopen(getenv("CHART_DUMP"), "w");
            if (df) { fputs(dsvg, df); fclose(df); }
            free(dsvg);
        }
        chart_free(dc);
    }

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuchart (bar/line/pie/scatter + formula subtitle, wubusvg-validated)\n");
    return 0;
}
