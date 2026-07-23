#ifndef WUBU_LEXICON_H
#define WUBU_LEXICON_H
/* lexicon.h -- dependency-free C11 word lexicon for WuBuOCR.
 *
 * Loads a certifiable frequency wordlist ("word<TAB>count" per line, ranked by
 * descending frequency; source: hermitdave/FrequencyWords OpenSubtitles 2018,
 * MIT code / CC-BY-SA-4.0 content). Provides:
 *   - frequency-proportional (Zipf) O(1) word sampling via Vose's alias method
 *     (same principle tokenizers use: common sequences dominate the training mix)
 *   - a character trie for lexicon-constrained decoding / spell-correction
 *   - UTF-8 <-> codepoint helpers and a per-lexicon charset (class map)
 *
 * Opaque struct; no globals; caller owns lifetime. No SIMD, no external deps.
 */
#include <stddef.h>
#include <stdint.h>

typedef struct Lexicon Lexicon;

/* Load up to `max_words` entries from a "word\tcount\n" file. If max_words<=0,
 * load all. Returns NULL on failure. */
Lexicon *lex_load(const char *path, int max_words);
void      lex_free(Lexicon *lx);

/* Counts. */
int    lex_size(const Lexicon *lx);           /* number of words */
int    lex_charset_size(const Lexicon *lx);   /* distinct codepoints seen */

/* Word access. Returns internal UTF-8 pointer (do NOT free); NULL if out of range. */
const char *lex_word(const Lexicon *lx, int idx);
uint64_t    lex_count(const Lexicon *lx, int idx);

/* Frequency-proportional sampler (Vose alias, O(1) per draw). rng is a caller
 * xorshift state (any nonzero seed). Returns a word index. */
int lex_sample(const Lexicon *lx, uint32_t *rng);
/* Uniform sample over the loaded words (ignores frequency). */
int lex_sample_uniform(const Lexicon *lx, uint32_t *rng);

/* Charset / class map: each distinct codepoint gets a class id in [1..K]
 * (0 reserved for CTC blank). Ordered by codepoint. */
int      lex_charset(const Lexicon *lx, uint32_t *out_cps, int cap); /* fills sorted cps, returns K */
int      lex_class_of(const Lexicon *lx, uint32_t cp);              /* 1..K, or 0 if absent */
uint32_t lex_cp_of_class(const Lexicon *lx, int cls);              /* codepoint for class, 0 if bad */

/* Trie lookup: exact membership. Returns word index or -1. */
int lex_contains(const Lexicon *lx, const char *utf8);

/* Nearest lexicon word to a (possibly noisy) UTF-8 string by Levenshtein
 * distance over codepoints, searching only words within +-len_slack length.
 * Returns best word index and writes distance to *out_dist (may be NULL).
 * O(candidates * L^2) — cheap for short OCR tokens. -1 if empty. */
int lex_correct(const Lexicon *lx, const char *utf8, int len_slack, int *out_dist);

/* UTF-8 helpers. Decode one codepoint at s; returns bytes consumed (0 at NUL/err),
 * writes codepoint to *cp. */
int utf8_decode(const char *s, uint32_t *cp);
/* Encode cp into buf (>=5 bytes); returns bytes written. */
int utf8_encode(uint32_t cp, char *buf);
/* Count codepoints in a UTF-8 string. */
int utf8_len(const char *s);
/* NFKC-lite: fold fullwidth/halfwidth + common compatibility forms in place.
 * See lexicon.c. Returns bytes written. */
int utf8_nfkc(const char *in, char *out, int cap);

#endif
