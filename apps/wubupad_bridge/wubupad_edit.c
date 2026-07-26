/* wubupad_edit.c -- CLI proving WuBuOffice reuses WuBuPad's editor core.
 * Usage:
 *   wubupad_edit <file>                 # print line/char stats
 *   wubupad_edit <file> --find PAT [--regex] [--icase] --repl STR
 *                                        # find/replace-all, print result
 * Demonstrates the cross-repo bridge (Phase E of the blitz). */
#include "wubupad_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file> [--find PAT --repl STR [--regex] [--icase]]\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];
    const char *find = NULL, *repl = NULL;
    int regex = 0, icase = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--find") == 0 && i + 1 < argc) find = argv[++i];
        else if (strcmp(argv[i], "--repl") == 0 && i + 1 < argc) repl = argv[++i];
        else if (strcmp(argv[i], "--regex") == 0) regex = 1;
        else if (strcmp(argv[i], "--icase") == 0) icase = 1;
    }

    if (find && repl) {
        char *out = NULL; size_t len = 0;
        int rc = wubupad_find_replace(path, find, regex, icase, repl, &out, &len);
        if (rc < 0) { fprintf(stderr, "error: cannot open %s\n", path); return 1; }
        fwrite(out, 1, len, stdout);
        if (rc == 1) fprintf(stderr, "\n[wubupad_edit] no matches\n");
        free(out);
        return 0;
    }

    size_t lines = 0, chars = 0;
    if (wubupad_stats(path, &lines, &chars) != 0) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return 1;
    }
    printf("%s: %zu lines, %zu chars (via WuBuPad editor core)\n", path, lines, chars);
    return 0;
}
