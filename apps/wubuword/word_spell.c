/* word_spell.c -- spell-check subcommand for wubuword.
 *
 * Wires the dependency-free wubuspell engine into the word processor: scans a
 * UTF-8 text file, reports each misspelled word with its line/column and a few
 * ranked suggestions. A user dictionary (one word per line) can be supplied to
 * extend the built-in seed list. */
#include "spell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Scan a single line and print any misspellings with suggestions. */
static int spell_line(SpellDict *d, const char *line, int lineno) {
    SpellError errs[128];
    int ne = spell_scan(d, line, errs, 128);
    int shown = ne < 128 ? ne : 128;
    for (int i = 0; i < shown; i++) {
        char word[256];
        int l = errs[i].len < 255 ? errs[i].len : 255;
        memcpy(word, line + errs[i].offset, (size_t)l);
        word[l] = 0;
        printf("  L%d:%d  %s", lineno, errs[i].offset + 1, word);
        char *sug[5];
        int ns = spell_suggest(d, word, sug, 5);
        if (ns > 0) {
            printf("  ->");
            for (int k = 0; k < ns; k++) { printf(" %s", sug[k]); free(sug[k]); }
        }
        printf("\n");
    }
    return ne;
}

/* wubuword spell <textfile> [userdict] */
int wubuword_spell_main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s spell <textfile> [userdict]\n", argv[0]);
        return 2;
    }
    const char *textpath = argv[2];
    const char *dictpath = (argc > 3) ? argv[3] : NULL;

    SpellDict *d = spell_create();
    if (!d) { fprintf(stderr, "spell: out of memory\n"); return 1; }
    spell_seed_english(d);
    if (dictpath) {
        int loaded = spell_load(d, dictpath);
        if (loaded < 0) fprintf(stderr, "spell: could not read dict %s\n", dictpath);
        else fprintf(stderr, "spell: loaded %d words from %s\n", loaded, dictpath);
    }

    FILE *f = fopen(textpath, "rb");
    if (!f) { fprintf(stderr, "spell: cannot open %s\n", textpath); spell_free(d); return 1; }

    char line[4096];
    int lineno = 0, total = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        total += spell_line(d, line, lineno);
    }
    fclose(f);

    printf("spell: %d misspelling(s) in %d line(s) of %s\n", total, lineno, textpath);
    spell_free(d);
    return total > 0 ? 0 : 0;   /* report is informational, not an error code */
}
