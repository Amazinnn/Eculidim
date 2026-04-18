#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void lex_init(Lexer *L, const char *input) {
    L->input = input; L->pos = 0; L->len = strlen(input);
    L->has_peek = 0; L->err[0] = 0;
    lex_next(L);
}

void lex_free(Lexer *L) { (void)L; }

static int is_alpha_(char c) {
    return isalpha(c) || c == '_';
}

static int is_alnum_(char c) {
    return isalnum(c) || c == '_';
}

static void skip_ws(Lexer *L) {
    while (L->pos < L->len && isspace(L->input[L->pos])) L->pos++;
}

static Token make_err(const char *msg) {
    Token t = {TK_ERR, {0}, 0, 0, 0};
    strncpy(t.text, msg, sizeof(t.text) - 1);
    return t;
}

Token lex_next(Lexer *L) {
    if (L->has_peek) { L->has_peek = 0; return L->cur; }
    if (L->pos >= L->len) {
        L->cur = (Token){TK_EOF, {0}, 0, L->pos, 0};
        return L->cur;
    }
    skip_ws(L);
    if (L->pos >= L->len) { L->cur = (Token){TK_EOF, {0}, 0, L->pos, 0}; return L->cur; }

    int start = L->pos;
    char c = L->input[L->pos];

    if (c == '+') { L->pos++; L->cur = (Token){TK_PLUS, {"+"}, 0, start, 1}; return L->cur; }
    if (c == '-') { L->pos++; L->cur = (Token){TK_MINUS, {"-"}, 0, start, 1}; return L->cur; }
    if (c == '*') { L->pos++; L->cur = (Token){TK_MUL, {"*"}, 0, start, 1}; return L->cur; }
    if (c == '/') { L->pos++; L->cur = (Token){TK_DIV, {"/"}, 0, start, 1}; return L->cur; }
    if (c == '(') { L->pos++; L->cur = (Token){TK_LPAREN, {"("}, 0, start, 1}; return L->cur; }
    if (c == ')') { L->pos++; L->cur = (Token){TK_RPAREN, {")"}, 0, start, 1}; return L->cur; }
    if (c == '{') { L->pos++; L->cur = (Token){TK_LBRACE, {"{"}, 0, start, 1}; return L->cur; }
    if (c == '}') { L->pos++; L->cur = (Token){TK_RBRACE, {"}"}, 0, start, 1}; return L->cur; }
    if (c == '[') { L->pos++; L->cur = (Token){TK_LBRACKET, {"["}, 0, start, 1}; return L->cur; }
    if (c == ']') { L->pos++; L->cur = (Token){TK_RBRACKET, {"]"}, 0, start, 1}; return L->cur; }
    if (c == '^') { L->pos++; L->cur = (Token){TK_CARET, {"^"}, 0, start, 1}; return L->cur; }
    if (c == '_') { L->pos++; L->cur = (Token){TK_UNDERSCORE, {"_"}, 0, start, 1}; return L->cur; }
    if (c == ',') { L->pos++; L->cur = (Token){TK_COMMA, {","}, 0, start, 1}; return L->cur; }
    if (c == '|') { L->pos++; L->cur = (Token){TK_PIPE, {"|"}, 0, start, 1}; return L->cur; }
    if (c == '=') { L->pos++; L->cur = (Token){TK_EQ, {"="}, 0, start, 1}; return L->cur; }
    if (c == '%') { L->pos++; L->cur = (Token){TK_PERCENT, {"%"}, 0, start, 1}; return L->cur; }
    if (c == '~') { L->pos++; L->cur = (Token){TK_TILDE, {"~"}, 0, start, 1}; return L->cur; }

    /* 数字 */
    if (isdigit(c) || (c == '.' && L->pos + 1 < L->len && isdigit(L->input[L->pos+1]))) {
        char buf[64]; int i = 0;
        while (L->pos < L->len && (isdigit(L->input[L->pos]) || L->input[L->pos] == '.')) {
            if (i < 63) buf[i++] = L->input[L->pos++];
        }
        buf[i] = 0;
        Token t = {TK_NUM, {0}, atof(buf), start, i};
        strncpy(t.text, buf, sizeof(t.text)-1);
        L->cur = t; return t;
    }

    /* 变量 a-z A-Z */
    if (is_alpha_(c)) {
        char buf[64]; int i = 0;
        while (L->pos < L->len && is_alnum_(L->input[L->pos])) {
            if (i < 63) buf[i++] = L->input[L->pos++];
        }
        buf[i] = 0;
        Token t = {TK_VAR, {0}, 0, start, i};
        strncpy(t.text, buf, sizeof(t.text)-1);
        L->cur = t; return t;
    }

    /* LaTeX 命令 \cmd */
    if (c == '\\') {
        int i = 0; L->pos++;
        char buf[64] = "\\";
        while (L->pos < L->len && is_alpha_(L->input[L->pos])) {
            if (i < 62) buf[++i] = L->input[L->pos++];
        }
        buf[i+1] = 0;
        Token t = {TK_CMD, {0}, 0, start, L->pos - start};
        strncpy(t.text, buf, sizeof(t.text)-1);
        L->cur = t; return t;
    }

    L->pos++;
    L->cur = make_err("未知字符");
    return L->cur;
}

Token lex_peek(Lexer *L) {
    if (!L->has_peek) { L->cur = lex_next(L); L->has_peek = 1; }
    return L->cur;
}

void lex_rewind(Lexer *L) { L->has_peek = 1; }
int lex_eof(const Lexer *L) { return L->cur.type == TK_EOF; }
const char* lex_cmd_name(const Token *t) { return t->type == TK_CMD ? t->text : ""; }
int lex_pos(const Lexer *L) { return L->cur.pos; }

const char* token_type_name(TokenType t) {
    switch (t) {
        case TK_NUM: return "NUM"; case TK_VAR: return "VAR"; case TK_OP: return "OP";
        case TK_LPAREN: return "LPAREN"; case TK_RPAREN: return "RPAREN";
        case TK_LBRACE: return "LBRACE"; case TK_RBRACE: return "RBRACE";
        case TK_LBRACKET: return "LBRACKET"; case TK_RBRACKET: return "RBRACKET";
        case TK_UNDERSCORE: return "UNDERSCORE"; case TK_CARET: return "CARET";
        case TK_COMMA: return "COMMA"; case TK_PIPE: return "PIPE";
        case TK_TILDE: return "TILDE";
        case TK_CMD: return "CMD"; case TK_CMD_MATH: return "CMD_MATH";
        case TK_EQ: return "EQ"; case TK_LOR: return "LOR"; case TK_LAND: return "LAND";
        case TK_PERCENT: return "PERCENT";
        case TK_PLUS: return "PLUS"; case TK_MINUS: return "MINUS";
        case TK_MUL: return "MUL"; case TK_DIV: return "DIV";
        case TK_EOF: return "EOF"; case TK_ERR: return "ERR";
        default: return "UNKNOWN";
    }
}
