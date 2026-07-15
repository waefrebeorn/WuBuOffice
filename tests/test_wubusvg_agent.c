/* test_wubusvg_agent.c -- NDJSON agent protocol for wubusvg.
 * Drives the same dispatcher wubuOS uses and asserts the JSON results. The
 * regurgitation result is also written to /tmp for an INDEPENDENT XML
 * well-formedness oracle. */
#include "agent.h"
#include "json.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

/* minimal JSON field assertions on an emitted result string */
static const char *has_field(const char *json, const char *key) {
    char needle[64];
    snprintf(needle, sizeof needle, "\"%s\"", key);
    return strstr(json, needle);
}

int main(void) {
    SvgAgent *a = svgagent_create();
    CK(a != NULL, "agent create");

    /* ingest a known SVG (single-quoted attrs avoid JSON escaping fragility) */
    const char *cmd =
        "{\"cmd\":\"ingest\",\"text\":\"<svg><g><rect x='1'/><text>t</text></g>"
        "<defs><font id='f'><glyph unicode='A' d='M0 0Z'/></font></defs></svg>\"}";

    char *r = svgagent_handle(a, cmd);
    CK(r != NULL, "ingest returns json");
    CK(has_field(r, "ok") != NULL, "ingest ok");
    CK(has_field(r, "root") != NULL, "ingest root field");
    CK(strstr(r, "\"glyphs\"") != NULL, "ingest reports glyphs");
    free(r);

    /* count */
    r = svgagent_handle(a, "{\"cmd\":\"count\",\"tag\":\"glyph\"}");
    CK(r && strstr(r, "\"count\":1") != NULL, "count glyph == 1");
    free(r);

    /* find + find_all */
    r = svgagent_handle(a, "{\"cmd\":\"find\",\"path\":\"g/rect\"}");
    CK(r && strstr(r, "\"found\":true") != NULL && strstr(r, "\"rect\""), "find g/rect");
    free(r);
    r = svgagent_handle(a, "{\"cmd\":\"find_all\",\"path\":\"glyph\"}");
    CK(r && strstr(r, "\"count\":1") != NULL, "find_all glyph == 1");
    free(r);

    /* set by path */
    r = svgagent_handle(a, "{\"cmd\":\"set\",\"path\":\"g/rect\",\"key\":\"class\",\"val\":\"x\"}");
    CK(r && strstr(r, "\"ok\":true") != NULL, "set by path ok");
    free(r);

    /* remove by path */
    r = svgagent_handle(a, "{\"cmd\":\"remove\",\"path\":\"g/text\"}");
    CK(r && strstr(r, "\"removed\":true") != NULL, "remove by path");
    free(r);

    /* error cases */
    r = svgagent_handle(a, "{\"cmd\":\"bogus\"}");
    CK(r && strstr(r, "\"error\"") != NULL, "unknown command -> error");
    free(r);
    r = svgagent_handle(a, "{\"cmd\":\"find\"}");
    CK(r && strstr(r, "\"error\"") != NULL, "missing field -> error");
    free(r);

    /* regurgitate -> independent well-formedness oracle */
    r = svgagent_handle(a, "{\"cmd\":\"regurgitate\"}");
    CK(r != NULL, "regurgitate returns json");
    const char *svg_field = strstr(r, "\"svg\":\"");
    CK(svg_field != NULL, "regurgitate has svg field");
    if (svg_field) {
        /* extract the SVG string (double-quoted, JSON-escaped) up to closing
         * quote; unescape is overkill for our check — write the raw json's
         * inner value via a tiny parse using our own json lib. */
        JVal *res = j_parse(r, NULL);
        const JVal *sv = res ? j_obj_get(res, "svg") : NULL;
        if (sv && j_type(sv) == J_STR) {
            FILE *tf = fopen("/tmp/wubusvg_agent_out.svg", "wb");
            if (tf) { fputs(j_as_str(sv), tf); fclose(tf); }
        }
        j_free(res);
    }
    free(r);

    /* quit */
    r = svgagent_handle(a, "{\"cmd\":\"quit\"}");
    CK(r && strstr(r, "\"ok\":true") != NULL, "quit ok");
    free(r);

    svgagent_free(a);

    if (fails) { printf("\nWUBUSVG_AGENT TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUSVG_AGENT TESTS PASSED\n");
    return 0;
}
