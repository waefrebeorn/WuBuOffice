#include "docx_text.h"

#include <stdlib.h>
#include <string.h>

int wubuoxml_docx_text(const uint8_t *xml, size_t len, char **out) {
    const char *s = (const char *)xml;
    size_t cap = 1024, used = 0;
    char *buf = malloc(cap);
    if (!buf) return -1;
    buf[0] = '\0';

    /* state: 0 = outside w:t body, 1 = inside a <w:t ...> open tag (collecting
     * attrs), 2 = inside the text content between '>' and the closing '<'. */
    int state = 0;
    for (size_t i = 0; i < len; i++) {
        if (state == 0) {
            if (i + 4 <= len && strncmp(s + i, "<w:t", 4) == 0) {
                state = 1;
                i += 3;
            }
        } else if (state == 1) {
            if (s[i] == '>') { state = 2; }
            /* ignore attributes until the '>' */
        } else { /* state == 2 : text content */
            if (s[i] == '<') { state = 0; continue; }
            char c = s[i];
            if (c == '&') { /* minimal entity decode */
                if (i + 5 <= len && strncmp(s + i, "&amp;", 5) == 0) { c = '&'; i += 4; }
                else if (i + 4 <= len && strncmp(s + i, "&lt;", 4) == 0) { c = '<'; i += 3; }
                else if (i + 4 <= len && strncmp(s + i, "&gt;", 4) == 0) { c = '>'; i += 3; }
                else { continue; }
            }
            if (used + 2 >= cap) {
                cap *= 2;
                char *nb = realloc(buf, cap);
                if (!nb) { free(buf); return -1; }
                buf = nb;
            }
            buf[used++] = c;
        }
    }
    if (used + 1 >= cap) {
        char *nb = realloc(buf, used + 2);
        if (!nb) { free(buf); return -1; }
        buf = nb;
    }
    buf[used] = '\0';
    *out = buf;
    return 0;
}
