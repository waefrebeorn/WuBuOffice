#include "rels_path.h"

#include <string.h>

char *wubuoxml_rels_path_for(const char *source) {
    if (!source || source[0] == '\0') return strdup("_rels/.rels");
    const char *slash = strrchr(source, '/');
    size_t dirlen = slash ? (size_t)(slash - source) + 1 : 0;
    size_t baselen = slash ? strlen(slash + 1) : strlen(source);
    size_t need = dirlen + 6 /* "_rels/" */ + baselen + 5 /* ".rels" */ + 1;
    char *buf = malloc(need);
    if (!buf) return NULL;
    char *q = buf;
    if (dirlen) { memcpy(q, source, dirlen); q += dirlen; }
    memcpy(q, "_rels/", 6); q += 6;
    memcpy(q, slash ? slash + 1 : source, baselen); q += baselen;
    memcpy(q, ".rels", 5); q += 5;
    *q = '\0';
    return buf;
}
