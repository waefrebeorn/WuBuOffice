/* WuBuOffice -- apps/wubushow/show_model
 * Presentation model + per-slide XML rendering. The data model (slides) and
 * the slide/body serializers live here, separate from package assembly
 * (show_io.c). Public API in show.h; internals in show_internal.h.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "show_internal.h"

wubushow_pres *wubushow_create(void) { return calloc(1, sizeof(wubushow_pres)); }

int wubushow_slide(wubushow_pres *p, const char *title, const char *body) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 4;
        p->slides = realloc(p->slides, p->cap * sizeof(*p->slides));
    }
    slide_t *s = &p->slides[p->n++];
    s->title = strdup(title ? title : "");
    s->body = strdup(body ? body : "");
    return (int)p->n;
}

void show_xml_escape(FILE *m, const char *t) {
    for (; t && *t; t++) {
        switch (*t) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break; case '>': fputs("&gt;", m); break; case '"': fputs("&quot;", m); break; default: fputc(*t, m); }
    }
}

/* Emit body text as one or more paragraphs, splitting on '\n'. Each paragraph
 * gets a bullet run so the slide reads like a real outline. */
void show_render_body(FILE *m, const char *body) {
    const char *p = body ? body : "";
    const char *line = p;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        fprintf(m, "<a:p><a:pPr><a:buFont typeface=\"Arial\"/><a:buChar char=\"•\"/></a:pPr><a:r><a:t>");
        /* emit exactly `len` chars of the current line (manual escape) */
        for (size_t i = 0; i < len; i++) {
            char c = line[i];
            switch (c) { case '&': fputs("&amp;", m); break; case '<': fputs("&lt;", m); break;
                          case '>': fputs("&gt;", m); break; default: fputc(c, m); }
        }
        fprintf(m, "</a:t></a:r></a:p>\n");
        if (!nl) break;
        line = nl + 1;
    }
}

char *show_render_slide(const slide_t *s, int idx) {
    (void)idx; /* reserved for future per-slide numbering */
    char *b = NULL; size_t n = 0; FILE *m = open_memstream(&b, &n);
    fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    fprintf(m, "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">\n");
    fprintf(m, "<p:cSld><p:spTree>\n");
    fprintf(m, "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>\n");
    /* title */
    fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"2\" name=\"Title\"/><p:cNvSpPr><a:spLocks noGrp=\"1\"/></p:cNvSpPr><p:nvPr/></p:nvSpPr>\n");
    fprintf(m, "<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>");
    show_xml_escape(m, s->title);
    fprintf(m, "</a:t></a:r></a:p></p:txBody></p:sp>\n");
    /* body (one or more bullet paragraphs) */
    fprintf(m, "<p:sp><p:nvSpPr><p:cNvPr id=\"3\" name=\"Body\"/><p:cNvSpPr><a:spLocks noGrp=\"1\"/></p:cNvSpPr><p:nvPr/></p:nvSpPr>\n");
    fprintf(m, "<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>\n");
    show_render_body(m, s->body);
    fprintf(m, "</p:txBody></p:sp>\n");
    fprintf(m, "</p:spTree></p:cSld>\n");
    fprintf(m, "<p:clrMapOvr><a:overrideClrMapping masterClrMapping=\"1\"/></p:clrMapOvr>\n");
    fprintf(m, "</p:sld>\n");
    fflush(m); fclose(m);
    return b;
}
