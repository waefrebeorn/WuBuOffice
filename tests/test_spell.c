/* test_spell.c -- wubuspell acceptance test (dependency-free spell checker). */
#include "spell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

int main(void) {
    SpellDict *d = spell_create();
    CHECK(d != NULL, "spell_create");
    if (!d) return 1;

    int seeded = spell_seed_english(d);
    CHECK(seeded > 100, "seed english > 100 words");
    CHECK(spell_size(d) == seeded, "size matches seeded");

    /* membership (case-insensitive) */
    CHECK(spell_check(d, "the") == 1, "'the' known");
    CHECK(spell_check(d, "The") == 1, "'The' known (case-insensitive)");
    CHECK(spell_check(d, "DOCUMENT") == 1, "'DOCUMENT' known (upper)");
    CHECK(spell_check(d, "helllo") == 0, "'helllo' misspelled");
    CHECK(spell_check(d, "") == 1, "empty is not flagged");
    CHECK(spell_check(d, "2024") == 1, "pure number not flagged");
    CHECK(spell_check(d, "3.14") == 1, "decimal not flagged");

    /* add + dedupe */
    CHECK(spell_add_word(d, "wubuoffice") == 1, "add new word");
    CHECK(spell_add_word(d, "wubuoffice") == 0, "re-add returns 0");
    CHECK(spell_check(d, "WuBuOffice") == 1, "added word known (case fold)");

    /* ignore list */
    CHECK(spell_check(d, "zzqfoo") == 0, "unknown before ignore");
    spell_ignore(d, "zzqfoo");
    CHECK(spell_check(d, "ZZQFOO") == 1, "ignored word treated as known");

    /* suggestions: 'helllo' -> 'hello' */
    char *sug[8];
    int ns = spell_suggest(d, "helllo", sug, 8);
    CHECK(ns > 0, "suggestions returned for 'helllo'");
    int found_hello = 0;
    for (int i = 0; i < ns; i++) { if (strcmp(sug[i], "hello") == 0) found_hello = 1; free(sug[i]); }
    CHECK(found_hello, "'hello' among suggestions for 'helllo'");

    /* suggestions: 'documnet' -> 'document' */
    ns = spell_suggest(d, "documnet", sug, 8);
    int found_doc = 0;
    for (int i = 0; i < ns; i++) { if (strcmp(sug[i], "document") == 0) found_doc = 1; free(sug[i]); }
    CHECK(found_doc, "'document' suggested for 'documnet'");

    /* text scanning: byte offsets of misspelled spans */
    const char *text = "The quikc brown fx jumps.";  /* quikc, fx misspelled */
    SpellError errs[16];
    int ne = spell_scan(d, text, errs, 16);
    CHECK(ne == 2, "scan finds 2 errors");
    if (ne >= 1) {
        /* first error is "quikc" at offset 4, len 5 */
        char span[32];
        int l = errs[0].len < 31 ? errs[0].len : 31;
        memcpy(span, text + errs[0].offset, (size_t)l); span[l] = 0;
        CHECK(strcmp(span, "quikc") == 0, "first error span == 'quikc'");
    }

    /* a clean sentence yields no errors */
    CHECK(spell_scan(d, "the document is a good page", errs, 16) == 0, "clean sentence 0 errors");

    /* load from file */
    const char *tmp = "/tmp/wubuspell_words.txt";
    FILE *f = fopen(tmp, "w");
    if (f) {
        fprintf(f, "# comment\nfoobarbaz\tcount123\nquuxword\n\n");
        fclose(f);
        int loaded = spell_load(d, tmp);
        CHECK(loaded == 2, "loaded 2 words (comment + blank skipped)");
        CHECK(spell_check(d, "foobarbaz") == 1, "loaded word known (tab-split)");
        CHECK(spell_check(d, "quuxword") == 1, "second loaded word known");
        remove(tmp);
    }

    spell_free(d);

    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuspell (dict + suggest + scan + load)\n");
    return 0;
}
