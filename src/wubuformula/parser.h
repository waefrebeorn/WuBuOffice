#ifndef WUBUFORMULA_PARSER_H
#define WUBUFORMULA_PARSER_H

#include "ast.h"
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse a formula (the text WITHOUT a leading '='). Returns the AST root, or
 * NULL on syntax error. On error the parse records *err with a message. */
ast *wubu_parse(const char *src, size_t len, char *errbuf, size_t errcap);

#ifdef __cplusplus
}
#endif

#endif /* WUBUFORMULA_PARSER_H */
