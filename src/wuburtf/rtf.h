/* rtf.h -- rich-text (RTF) writer (EXP-87). Serializes a list of styled runs
 * to an RTF \ansi document. A run is text + flags (bold/italic/mono). Opaque. */
#ifndef WUBURTF_H
#define WUBURTF_H

typedef struct RtfRun {
    const char *text;
    int bold, italic, mono;
} RtfRun;

/* Write `n` runs to a malloc'd RTF string (caller frees). Returns NULL on
 * error. */
char *rtf_write(const RtfRun *runs, int n);

#endif /* WUBURTF_H */
