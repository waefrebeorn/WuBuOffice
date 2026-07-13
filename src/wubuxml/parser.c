/* parser.c — minimal streaming XML event parser (see parser.h).
 * Single-pass, constant memory. Emits START / END / TEXT events.
 * Namespaces are KEPT on names ("w:p"). Handles self-closing tags,
 * attributes (incl. namespaced), character data, and the common
 * entities (&amp; &lt; &gt; &quot; &apos;). */

#include "parser.h"
#include <stdlib.h>
#include <string.h>

/* Decode one entity starting at s[0]=='&'. Writes decoded char to *out,
 * returns bytes consumed (>=2), or 0 if not a recognized entity. */
static size_t decode_entity(const char *s, size_t maxlen, char *out) {
    if (maxlen < 2 || s[0] != '&') return 0;
    if (maxlen >= 5 && strncmp(s, "&amp;", 5) == 0) { *out = '&'; return 5; }
    if (maxlen >= 4 && strncmp(s, "&lt;", 4) == 0) { *out = '<'; return 4; }
    if (maxlen >= 4 && strncmp(s, "&gt;", 4) == 0) { *out = '>'; return 4; }
    if (maxlen >= 6 && strncmp(s, "&quot;", 6) == 0) { *out = '"'; return 6; }
    if (maxlen >= 6 && strncmp(s, "&apos;", 6) == 0) { *out = '\''; return 6; }
    return 0;
}

static char *dup_range(const char *a, const char *b) {
    size_t n = (size_t)(b - a);
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, a, n);
    p[n] = 0;
    return p;
}

static char *trim_inplace(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = 0;
    return s;
}

int wubuxml_parse(const uint8_t *data, size_t len,
                   wubuxml_handler h, void *user) {
    const char *s = (const char *)data;
    wubuxml_info info;
    memset(&info, 0, sizeof info);

    char *textbuf = NULL;       /* accumulated, entity-decoded text */
    size_t tcap = 0, tlen = 0;
    int in_text = 0;            /* between '>' and next '<' */

    size_t i = 0;
    char **stack = NULL;        /* open element names (owned) */
    size_t sp = 0, scap = 0;
    int rc = 0;

    /* helper: flush accumulated text as one EVT_TEXT */
#define FLUSH_TEXT() do { \
        if (tlen > 0 && h) { \
            textbuf[tlen] = 0; \
            info.name = NULL; info.attr_count = 0; \
            info.text = textbuf; info.text_len = tlen; \
            rc = h(WUBUXML_EVT_TEXT, &info, user); \
            tlen = 0; \
        } \
    } while (0)

    while (i < len && rc == 0) {
        if (s[i] == '<') {
            /* a tag opens: flush any pending text first */
            FLUSH_TEXT();
            size_t tag = i + 1;
            if (tag < len && s[tag] == '/') {
                /* Closing tag </name>: emit END in document order and pop
                 * our open-element stack. (Previously the parser swallowed
                 * every </...> and deferred all ENDs to a LIFO drain at EOF,
                 * which scrambled the event stream for nested/sibling
                 * elements — see ws05#0884 regression.) */
                size_t nm = tag + 1;
                while (nm < len && s[nm] != '>' && s[nm] != ' ') nm++;
                char *cname = dup_range(s + tag + 1, s + nm);
                if (!cname) { rc = -1; break; }
                info.name = cname;
                info.attr_count = 0;
                if (h) rc = h(WUBUXML_EVT_END, &info, user);
                free(cname);
                info.name = NULL;
                if (sp > 0) free(stack[--sp]);
                size_t e = nm;
                while (e < len && s[e] != '>') e++;
                i = (e < len) ? e + 1 : len;
                continue;
            }
            if (tag < len && (s[tag] == '!' || s[tag] == '?')) {
                /* comment / decl / PI: skip to '>' */
                size_t e = tag;
                while (e < len && s[e] != '>') e++;
                i = (e < len) ? e + 1 : len;
                continue;
            }
            size_t end = tag;
            while (end < len && s[end] != '>' && s[end] != '<') end++;
            if (end >= len) { rc = -1; break; }
            int self_close = 0;
            if (end > tag && s[end - 1] == '/') { self_close = 1; end--; }

            /* element name = first token (guard: p must pass tag) */
            size_t p = tag;
            while (p < end && s[p] != ' ' && s[p] != '\t' && s[p] != '\n' &&
                   s[p] != '\r' && s[p] != '/') p++;
            if (p <= tag) { rc = -1; break; }   /* empty name -> malformed */
            char *ename = dup_range(s + tag, s + p);
            if (!ename) { rc = -1; break; }

            /* attributes */
            info.attr_count = 0;
            while (p < end && info.attr_count < WUBUXML_MAX_ATTR) {
                while (p < end && (s[p] == ' ' || s[p] == '\t' ||
                       s[p] == '\n' || s[p] == '\r')) p++;
                if (p >= end) break;
                size_t an = p;
                while (p < end && s[p] != '=' && s[p] != ' ' && s[p] != '\t') p++;
                if (p <= an) { free(ename); rc = -1; break; } /* no '=' -> malformed */
                char *attrn = dup_range(s + an, s + p);
                if (!attrn) { free(ename); rc = -1; break; }
                while (p < end && s[p] != '=') p++;
                if (p >= end) { free(attrn); free(ename); rc = -1; break; }
                p++;
                while (p < end && (s[p] == ' ' || s[p] == '\t')) p++;
                char q = (p < end) ? s[p] : '"';
                if (q != '"' && q != '\'') { free(attrn); free(ename); rc = -1; break; }
                p++;
                size_t av = p;
                while (p < end && s[p] != q) p++;
                if (p <= av) { free(attrn); free(ename); rc = -1; break; } /* empty value */
                char *attrv = dup_range(s + av, s + p);
                if (!attrv) { free(attrn); free(ename); rc = -1; break; }
                info.attr_name[info.attr_count] = trim_inplace(attrn);
                info.attr_val[info.attr_count] = trim_inplace(attrv);
                info.attr_count++;
                if (p < end) p++;
            }
            if (rc != 0) break;

            info.name = ename;
            if (h) rc = h(WUBUXML_EVT_START, &info, user);

            if (self_close) {
                /* end was decremented to point at the '/', so end+1 is the
                 * '>'. Skip it (end+2) so the '>' is NOT consumed as pending
                 * text. (Previously end+1 left i on '>', leaking '>' into the
                 * character-data stream — ws05#0884.) */
                if (!rc && h) {
                    info.name = ename;
                    rc = h(WUBUXML_EVT_END, &info, user);
                }
                i = end + 2;
            } else {
                if (sp == scap) {
                    scap = scap ? scap * 2 : 8;
                    char **ns = realloc(stack, scap * sizeof *ns);
                    if (!ns) { free(ename); rc = -1; break; }
                    stack = ns;
                }
                stack[sp++] = ename;
                ename = NULL;
                i = end + 1;
            }
            in_text = 1;          /* text may follow the '>' */
            if (ename) free(ename);
            for (int k = 0; k < info.attr_count; k++) {
                free((void *)info.attr_name[k]);
                free((void *)info.attr_val[k]);
            }
            info.attr_count = 0;
            info.name = NULL;
        } else {
            /* character data */
            if (in_text) {
                char c = s[i];
                if (c == '&') {
                    char dec;
                    size_t n = decode_entity(s + i, len - i, &dec);
                    if (n) {
                        if (tlen + 2 > tcap) {
                            tcap = tlen + 16;
                            char *nb = realloc(textbuf, tcap);
                            if (!nb) { rc = -1; break; }
                            textbuf = nb;
                        }
                        textbuf[tlen++] = dec;
                        i += n;
                        continue;
                    }
                }
                if (tlen + 2 > tcap) {
                    tcap = tlen + 16;
                    char *nb = realloc(textbuf, tcap);
                    if (!nb) { rc = -1; break; }
                    textbuf = nb;
                }
                textbuf[tlen++] = c;
            }
            i++;
        }
    }
    FLUSH_TEXT();

    /* emit END for any still-open elements */
    if (rc == 0) {
        while (sp > 0 && rc == 0) {
            char *nm = stack[--sp];
            info.name = nm;
            info.attr_count = 0;
            if (h) rc = h(WUBUXML_EVT_END, &info, user);
            free(nm);
        }
    } else {
        for (size_t k = 0; k < sp; k++) free(stack[k]);
    }
    free(stack);
    free(textbuf);
    return rc;
}
