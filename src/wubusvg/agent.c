/* agent.c -- wubuOS NDJSON protocol dispatcher over the verified SVG core.
 * Self-contained C11; reuses wubusvg + wubujson (same API as WuBuPad's JSON so
 * both tools speak one wubuOS dialect). No GUI. */
#define _POSIX_C_SOURCE 200809L  /* getline() */
#include "agent.h"
#include "wubusvg.h"
#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct SvgAgent {
    SvgDoc *doc;   /* current ingested document (one at a time) */
};

SvgAgent *svgagent_create(void) {
    SvgAgent *a = malloc(sizeof *a);
    if (!a) return NULL;
    a->doc = NULL;
    return a;
}

void svgagent_free(SvgAgent *a) {
    if (!a) return;
    svg_free(a->doc);
    free(a);
}

static char *err(const char *msg) {
    JVal *o = j_obj();
    j_obj_put(o, "error", j_str(msg));
    char *s = j_emit(o);
    j_free(o);
    return s;
}

/* Replace the session document with a freshly parsed one. Takes ownership of d
 * (frees any prior doc). Returns 0 on success, -1 if d is NULL. */
static int agent_set_doc(SvgAgent *a, SvgDoc *d) {
    if (!d) return -1;
    svg_free(a->doc);
    a->doc = d;
    return 0;
}

/* glyph count if the root subtree has a <glyph> family (font-SVG), else 0 */
static size_t doc_glyph_count(SvgAgent *a) {
    if (!a->doc) return 0;
    return svg_count_tag(svg_root(a->doc), "glyph");
}

char *svgagent_handle(SvgAgent *a, const char *command_json) {
    if (!a || !command_json) return err("null");
    const char *end = NULL;
    JVal *cmd = j_parse(command_json, &end);
    if (!cmd || j_type(cmd) != J_OBJ) { j_free(cmd); return err("bad json"); }
    const JVal *c = j_obj_get(cmd, "cmd");
    if (!c || j_type(c) != J_STR) { j_free(cmd); return err("missing cmd"); }
    const char *name = j_as_str(c);

    JVal *res = NULL;

    if (strcmp(name, "ingest") == 0 || strcmp(name, "open") == 0) {
        SvgDoc *d = NULL;
        if (strcmp(name, "ingest") == 0) {
            const JVal *text = j_obj_get(cmd, "text");
            if (!text || j_type(text) != J_STR) { j_free(cmd); return err("ingest: text required"); }
            d = svg_parse(j_as_str(text), strlen(j_as_str(text)));
        } else { /* open: read file path */
            const JVal *path = j_obj_get(cmd, "path");
            if (!path || j_type(path) != J_STR) { j_free(cmd); return err("open: path required"); }
            FILE *f = fopen(j_as_str(path), "rb");
            if (!f) { j_free(cmd); return err("open: cannot read file"); }
            if (fseek(f, 0, SEEK_END) != 0) { fclose(f); j_free(cmd); return err("open: seek failed"); }
            long n = ftell(f); rewind(f);
            if (n < 0) { fclose(f); j_free(cmd); return err("open: size failed"); }
            char *buf = malloc((size_t)n + 1);
            size_t rd = fread(buf, 1, (size_t)n, f); fclose(f);
            buf[rd] = '\0';
            d = svg_parse(buf, rd);
            free(buf);
        }
        if (!d) { j_free(cmd); return err("parse failed (not well-formed SVG/XML)"); }
        agent_set_doc(a, d);
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
        j_obj_put(res, "root", j_str(svg_node_name(svg_root(d))));
        size_t g = doc_glyph_count(a);
        if (g) j_obj_put(res, "glyphs", j_num((double)g));
    }
    else if (strcmp(name, "find") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        if (!p || j_type(p) != J_STR) { j_free(cmd); return err("find: path required"); }
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        SvgNode *n = svg_find(svg_root(a->doc), j_as_str(p));
        res = j_obj();
        j_obj_put(res, "found", j_bool(n != NULL));
        if (n) j_obj_put(res, "name", j_str(svg_node_name(n)));
    }
    else if (strcmp(name, "find_all") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        if (!p || j_type(p) != J_STR) { j_free(cmd); return err("find_all: path required"); }
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        SvgNode *tmp[256];
        size_t n = svg_find_all(svg_root(a->doc), j_as_str(p), tmp, 256);
        res = j_obj();
        j_obj_put(res, "count", j_num((double)n));
    }
    else if (strcmp(name, "count") == 0) {
        const JVal *t = j_obj_get(cmd, "tag");
        if (!t || j_type(t) != J_STR) { j_free(cmd); return err("count: tag required"); }
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        size_t n = svg_count_tag(svg_root(a->doc), j_as_str(t));
        res = j_obj();
        j_obj_put(res, "count", j_num((double)n));
    }
    else if (strcmp(name, "set") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        const JVal *k = j_obj_get(cmd, "key");
        const JVal *v = j_obj_get(cmd, "val");
        if (!p || !k || !v || j_type(p) != J_STR || j_type(k) != J_STR || j_type(v) != J_STR) {
            j_free(cmd); return err("set: path,key,val required");
        }
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        int rc = svg_set_attr_path(svg_root(a->doc), j_as_str(p), j_as_str(k), j_as_str(v));
        res = j_obj();
        j_obj_put(res, "ok", j_bool(rc == 0));
        if (rc != 0) j_obj_put(res, "error", j_str("no match"));
    }
    else if (strcmp(name, "remove") == 0) {
        const JVal *p = j_obj_get(cmd, "path");
        if (!p || j_type(p) != J_STR) { j_free(cmd); return err("remove: path required"); }
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        int rc = svg_remove_path(svg_root(a->doc), j_as_str(p));
        res = j_obj();
        j_obj_put(res, "ok", j_bool(rc == 1));
        j_obj_put(res, "removed", j_bool(rc == 1));
        if (rc < 0) j_obj_put(res, "error", j_str("no match or root"));
    }
    else if (strcmp(name, "regurgitate") == 0) {
        if (!a->doc) { j_free(cmd); return err("no document ingested"); }
        char *out = svg_regurgitate(a->doc);
        if (!out) { j_free(cmd); return err("regurgitate failed"); }
        res = j_obj();
        j_obj_put(res, "svg", j_str(out));
        free(out);
    }
    else if (strcmp(name, "quit") == 0) {
        res = j_obj();
        j_obj_put(res, "ok", j_bool(1));
    }
    else {
        j_free(cmd);
        return err("unknown command");
    }

    char *s = j_emit(res);
    j_free(res);
    j_free(cmd);
    return s;
}

int svgagent_serve(SvgAgent *a, FILE *in, FILE *out) {
    if (!a || !in || !out) return -1;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int rc = 0;
    while ((len = getline(&line, &cap, in)) != -1) {
        /* trim trailing newline/CR only; JSON may contain internal whitespace */
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        char *resp = svgagent_handle(a, line);
        if (!resp) { rc = -1; break; }
        fputs(resp, out);
        fputc('\n', out);
        fflush(out);
        free(resp);
        /* quit terminates the loop */
        if (strncmp(line, "{\"cmd\":\"quit\"", 12) == 0) break;
    }
    free(line);
    return rc;
}
