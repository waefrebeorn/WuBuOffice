/* pasteplain.h -- paste-plain (EXP-88): strip rich-text markup, returning the
 * raw text content. Opaque helper. */
#ifndef WUBUPASTEPLAIN_H
#define WUBUPASTEPLAIN_H

/* Strip a run of markup; `in` may be RTF, HTML, or already-plain. Returns a
 * malloc'd plain-text copy (caller frees). The heuristic: drop content between
 * '<' '>' (HTML/XML tags) and between '{' '}' that follows a backslash (RTF
 * control words), keep everything else. */
char *pasteplain_strip(const char *in);

#endif /* WUBUPASTEPLAIN_H */
