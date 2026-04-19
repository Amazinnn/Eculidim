#ifndef ECULID_LEXER_H
#define ECULID_LEXER_H

#include <stddef.h>

typedef enum {
    TK_NUM, TK_VAR, TK_OP,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
    TK_UNDERSCORE, TK_CARET, TK_COMMA, TK_PIPE, TK_TILDE,
    TK_CMD, TK_CMD_MATH, TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_LOR, TK_LAND, TK_PERCENT,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_EOF, TK_ERR
} TokenType;

typedef struct {
    TokenType type;
    char text[128];
    double num;
    int pos;
    int len;
} Token;

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    Token cur;
    int has_peek;
    char err[128];
} Lexer;

void  lex_init(Lexer *L, const char *input);
void  lex_free(Lexer *L);
Token lex_next(Lexer *L);
Token lex_peek(Lexer *L);
void  lex_rewind(Lexer *L);
int   lex_eof(const Lexer *L);
const char* lex_cmd_name(const Token *t);
const char* token_type_name(TokenType t);
int   lex_pos(const Lexer *L);

#endif /* ECULID_LEXER_H */
