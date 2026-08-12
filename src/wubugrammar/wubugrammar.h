/* wubugrammar.h — rule-based grammar & consistency checker. Detects common
 * style/grammar issues: doubled words, a/an before vowel/consonant, common
 * misspellings/word-usage pairs, sentence spacing, contraction misuse. */
#ifndef WUBUGRAMMAR_H
#define WUBUGRAMMAR_H
#include <stddef.h>

typedef struct {
    int start, len;    /* byte offsets into the text */
    int issue_id;
    char message[160]; /* human-readable suggestion */
} wubugrammar_finding;

/* Check `text` and write up to `cap` findings into `out`. Returns the number
 * of findings (may exceed cap). Text is a NUL-terminated UTF-8/ASCII string. */
int wubugrammar_check(const char *text, wubugrammar_finding *out, int cap);

/* Re-seed the built-in misspelling/usage dictionary (optional). */
int wubugrammar_add_pair(const char *wrong, const char *right);

#endif
