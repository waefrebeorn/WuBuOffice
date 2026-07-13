#include "xml.h"

#include <stdlib.h>
#include <string.h>

struct wubuxml_writer {
    FILE *out;
    char **stack;
    size_t sp;
    size_t cap;
    int top_open; /* is the top element's start tag still unclosed ('>')? */
};

wubuxml_writer *wubuxml_create(FILE *out) {
    wubuxml_writer *w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->out = out;
    w->cap = 16;
    w->stack = malloc(w->cap * sizeof(char *));
    return w;
}

static void xml_escape(FILE *out, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&':  fputs("&amp;", out);  break;
            case '<':  fputs("&lt;", out);   break;
            case '>':  fputs("&gt;", out);   break;
            case '"':  fputs("&quot;", out); break;
            case '\'': fputs("&apos;", out); break;
            default:   fputc(*s, out);       break;
        }
    }
}

static void close_start(wubuxml_writer *w) {
    if (w->top_open) {
        fputc('>', w->out);
        w->top_open = 0;
    }
}

void wubuxml_declaration(wubuxml_writer *w) {
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n", w->out);
}

void wubuxml_open(wubuxml_writer *w, const char *name) {
    close_start(w); /* a child forces the parent's start tag to close */
    if (w->sp == w->cap) {
        w->cap *= 2;
        w->stack = realloc(w->stack, w->cap * sizeof(char *));
    }
    w->stack[w->sp++] = strdup(name);
    fputc('<', w->out);
    fputs(name, w->out);
    w->top_open = 1;
}

void wubuxml_set_attr(wubuxml_writer *w, const char *key, const char *val) {
    fputc(' ', w->out);
    fputs(key, w->out);
    fputc('=', w->out);
    fputc('"', w->out);
    xml_escape(w->out, val);
    fputc('"', w->out);
}

void wubuxml_text(wubuxml_writer *w, const char *text) {
    close_start(w);
    xml_escape(w->out, text);
}

void wubuxml_close(wubuxml_writer *w) {
    if (w->sp == 0) return;
    char *name = w->stack[--w->sp];
    if (w->top_open) {
        fputs("/>", w->out);
        w->top_open = 0;
    } else {
        fputc('<', w->out);
        fputc('/', w->out);
        fputs(name, w->out);
        fputc('>', w->out);
    }
    free(name);
}

void wubuxml_close_all(wubuxml_writer *w) {
    while (w->sp > 0) wubuxml_close(w);
}

void wubuxml_destroy(wubuxml_writer *w) {
    wubuxml_close_all(w);
    free(w->stack);
    free(w);
}
