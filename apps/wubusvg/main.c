/* wubusvg main -- ingest an SVG file into the document model and regurgitate it
 * as well-formed SVG. The AGI/WuBuOS ingestion hook for vector documents:
 *   wubusvg_cli in.svg            -> regurgitated SVG on stdout
 *   wubusvg_cli in.svg --count g  -> count of <g> elements (subtree inspect)
 * Native C11 + POSIX. */
#include "wubusvg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <in.svg> [--count <tag>]\n", argv[0]);
        return 2;
    }
    size_t len = 0;
    char *data = read_all(argv[1], &len);
    if (!data) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    SvgDoc *doc = svg_parse(data, len);
    free(data);
    if (!doc) { fprintf(stderr, "not a well-formed SVG/XML document\n"); return 1; }

    if (argc >= 4 && strcmp(argv[2], "--count") == 0) {
        size_t c = svg_count_tag(svg_root(doc), argv[3]);
        printf("%zu\n", c);
        svg_free(doc);
        return 0;
    }

    char *out = svg_regurgitate(doc);
    svg_free(doc);
    if (!out) { fprintf(stderr, "regurgitate failed\n"); return 1; }
    fputs(out, stdout);
    free(out);
    return 0;
}
