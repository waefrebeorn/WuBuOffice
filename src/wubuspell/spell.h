/* spell.h -- dependency-free C11 spell checker for WuBuOffice.
 *
 * A self-contained (SLERM: no third-party) spell-checking engine: a hash-set
 * dictionary, a UTF-8 word tokenizer that returns misspelled spans, and
 * Levenshtein-ranked suggestions. Hunspell/Nuspell are C++ and pull large
 * dependency trees; this stays pure C11, opaque struct, no globals.
 *
 * Dictionary words are matched case-insensitively for ASCII (so "The" matches
 * "the"); non-ASCII bytes are compared as-is. Load a newline-delimited word
 * list with spell_load(), or add words programmatically.
 */
#ifndef WUBUOFFICE_SPELL_H
#define WUBUOFFICE_SPELL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpellDict SpellDict;

/* A misspelled span within the scanned text (byte offsets). */
typedef struct {
    int offset;   /* byte offset of the word start */
    int len;      /* byte length of the word */
} SpellError;

/* Create an empty dictionary. Returns NULL on OOM. */
SpellDict *spell_create(void);
void       spell_free(SpellDict *d);

/* Add one word (copied, normalized). Returns 1 if newly added, 0 if present. */
int spell_add_word(SpellDict *d, const char *word);

/* Load a newline-delimited word list from `path`. Blank lines and lines
 * beginning with '#' are skipped; a "word\tcount" line keeps only the word.
 * Returns the number of words added, or -1 on file error. */
int spell_load(SpellDict *d, const char *path);

/* Number of words in the dictionary. */
int spell_size(const SpellDict *d);

/* 1 if `word` is known (case-insensitive for ASCII), else 0. A NULL/empty
 * word is treated as known (nothing to flag). Pure numbers are known. */
int spell_check(SpellDict *d, const char *word);

/* Add `word` to the session ignore list (treated as known until freed). */
void spell_ignore(SpellDict *d, const char *word);

/* Suggestions for a misspelled `word`, ranked by edit distance then by
 * dictionary insertion order. Writes up to `max` heap strings into out[]
 * (caller frees each with free()). Returns the count written. */
int spell_suggest(SpellDict *d, const char *word, char **out, int max);

/* Scan a UTF-8 string, tokenizing into words (letters + apostrophes), and
 * write misspelled spans into out[] (up to `max`). Returns the number of
 * errors found (may exceed `max`; only `max` are written). */
int spell_scan(SpellDict *d, const char *utf8, SpellError *out, int max);

/* Seed the dictionary with a small built-in English word list so the checker
 * is useful with zero configuration. Returns words added. */
int spell_seed_english(SpellDict *d);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_SPELL_H */
