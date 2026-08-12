#include "wubumailexport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int wubumailexport_build(wubumailexport *m, const char *to, const char *from,
                         const char *subject, const char *body){
    if (!m || !to || !from || !subject || !body) return -1;
    strncpy(m->to, to, sizeof m->to - 1); m->to[sizeof m->to - 1] = 0;
    strncpy(m->from, from, sizeof m->from - 1); m->from[sizeof m->from - 1] = 0;
    strncpy(m->subject, subject, sizeof m->subject - 1); m->subject[sizeof m->subject - 1] = 0;
    free(m->body);
    m->body = strdup(body);
    if (!m->body) return -1;
    m->bodylen = strlen(body);
    return 0;
}

char *wubumailexport_render(const wubumailexport *m){
    if (!m || !m->body) return NULL;
    char datebuf[64];
    time_t t = time(NULL);
    struct tm tmv;
    if (!localtime_r(&t, &tmv)) strcpy(datebuf, "?");
    else strftime(datebuf, sizeof datebuf, "%a, %d %b %Y %H:%M:%S %z", &tmv);
    /* size: headers + date + blank line + body */
    size_t need = 64 + sizeof m->to + sizeof m->from + sizeof m->subject + m->bodylen + 16;
    char *out = (char*)malloc(need);
    if (!out) return NULL;
    int n = snprintf(out, need,
        "To: %s\r\nFrom: %s\r\nSubject: %s\r\nDate: %s\r\n\r\n%s",
        m->to, m->from, m->subject, datebuf, m->body);
    if (n < 0) { free(out); return NULL; }
    return out;
}

void wubumailexport_free(wubumailexport *m){
    if (!m) return;
    free(m->body);
    m->body = NULL;
    m->bodylen = 0;
}
