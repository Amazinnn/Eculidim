#ifndef ECULID_PARSER_H
#define ECULID_PARSER_H

#include "eculid_ast.h"
#include "eculid_lexer.h"

typedef struct { Lexer *lexer; char err[256]; int err_pos; } Parser;

void   parser_init(Parser *P, const char *input);
Expr*  parser_parse(Parser *P);
Expr*  parser_parse_op(Parser *P);
int    parser_error(const Parser *P);
void   parser_free(Parser *P);
Expr*  ec_parse(const char *latex);
void   ec_parse_free(Expr *e);
int    ec_parse_error(void);
const char* ec_parse_error_msg(void);
int    ec_parse_error_pos(void);

#endif /* ECULID_PARSER_H */
