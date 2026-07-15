/* wubusvg main -- ingest an SVG file into the document model and regurgitate it
 * as well-formed SVG. The AGI/WuBuOS ingestion hook for vector documents:
 *   wubusvg_cli in.svg                          -> regurgitated SVG on stdout
 *   wubusvg_cli in.svg --count <tag>            -> count <tag> in subtree
 *   wubusvg_cli in.svg --find <path>            -> first node's tag (or "")
 *   wubusvg_cli in.svg --find-all <path>        -> count of matches
 *   wubusvg_cli in.svg --set <path> <k> <v>     -> set attr on first match
 *   wubusvg_cli in.svg --remove <path>          -> remove first match
 *   wubusvg_cli in.svg --set-attr <k> <v>        -> set attr on root
 *   wubusvg_cli in.svg --remove-attr <k>        -> remove attr on root
 * Paths use '/' between tag names, e.g. "g/rect" or "svg/g/rect".
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
        fprintf(stderr,
            "usage: %s <in.svg> [commands...]\n"
            "  default: regurgitate the (optionally edited) SVG to stdout.\n"
            "  commands: --count <tag> | --find <path> | --find-all <path> |\n"
            "            --set <path> <k> <v> | --remove <path> |\n"
            "            --set-attr <k> <v> | --remove-attr <k>\n", argv[0]);
        return 2;
    }
    size_t len = 0;
    char *data = read_all(argv[1], &len);
    if (!data) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    SvgDoc *doc = svg_parse(data, len);
    free(data);
    if (!doc) { fprintf(stderr, "not a well-formed SVG/XML document\n"); return 1; }

    int produced = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            printf("%zu\n", svg_count_tag(svg_root(doc), argv[i+1])); produced = 1; i += 1;
        } else if (strcmp(argv[i], "--find") == 0 && i + 1 < argc) {
            SvgNode *n = svg_find(svg_root(doc), argv[i+1]);
            printf("%s\n", n ? svg_node_name(n) : ""); produced = 1; i += 1;
        } else if (strcmp(argv[i], "--find-all") == 0 && i + 1 < argc) {
            SvgNode *tmp[256];
            printf("%zu\n", svg_find_all(svg_root(doc), argv[i+1], tmp, 256)); produced = 1; i += 1;
        } else if (strcmp(argv[i], "--set") == 0 && i + 3 < argc) {
            svg_set_attr_path(svg_root(doc), argv[i+1], argv[i+2], argv[i+3]); i += 3;
        } else if (strcmp(argv[i], "--remove") == 0 && i + 1 < argc) {
            svg_remove_path(svg_root(doc), argv[i+1]); i += 1;
        } else if (strcmp(argv[i], "--set-attr") == 0 && i + 2 < argc) {
            svg_set_attr(svg_root(doc), argv[i+1], argv[i+2]); i += 2;
        } else if (strcmp(argv[i], "--remove-attr") == 0 && i + 1 < argc) {
            svg_remove_attr(svg_root(doc), argv[i+1]); i += 1;
        }
    }

    if (produced) { svg_free(doc); return 0; }

    char *out = svg_regurgitate(doc);
    svg_free(doc);
    if (!out) { fprintf(stderr, "regurgitate failed\n"); return 1; }
    fputs(out, stdout);
    free(out);
    return 0;
}
