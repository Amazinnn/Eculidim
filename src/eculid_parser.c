#include "eculid.h"

static Parser g_parser;

void parser_init(Parser *P, const char *input) {
    static Lexer Lx;
    lex_init(&Lx, input);
    P->lexer = &Lx; P->err[0] = 0; P->err_pos = 0;
}

/* 前向声明 */
static Expr* parse_expr(Parser *P);
static Expr* parse_addsub(Parser *P);
static Expr* parse_muldiv(Parser *P);
static Expr* parse_pow(Parser *P);
static Expr* parse_unary(Parser *P);
static Expr* parse_primary(Parser *P);

static Expr* parse_expr(Parser *P) { return parse_addsub(P); }

static Expr* parse_addsub(Parser *P) {
    Expr *left = parse_muldiv(P);
    while (P->lexer->cur.type == TK_PLUS || P->lexer->cur.type == TK_MINUS) {
        TokenType op = P->lexer->cur.type;
        lex_next(P->lexer);
        Expr *right = parse_muldiv(P);
        left = ec_binary(op == TK_PLUS ? EC_ADD : EC_SUB, left, right);
    }
    return left;
}

static Expr* parse_muldiv(Parser *P) {
    Expr *left = parse_pow(P);
    while (P->lexer->cur.type == TK_MUL || P->lexer->cur.type == TK_DIV) {
        TokenType op = P->lexer->cur.type;
        lex_next(P->lexer);
        Expr *right = parse_pow(P);
        left = ec_binary(op == TK_MUL ? EC_MUL : EC_DIV, left, right);
    }
    return left;
}

static Expr* parse_pow(Parser *P) {
    Expr *base = parse_unary(P);
    if (P->lexer->cur.type == TK_CARET) {
        lex_next(P->lexer);
        Expr *exp = parse_pow(P); /* 右结合 */
        return ec_binary(EC_POW, base, exp);
    }
    return base;
}

static Expr* parse_unary(Parser *P) {
    if (P->lexer->cur.type == TK_MINUS) {
        lex_next(P->lexer);
        return ec_unary(EC_NEG, parse_unary(P));
    }
    if (P->lexer->cur.type == TK_PLUS) {
        lex_next(P->lexer);
        return parse_unary(P);
    }
    return parse_primary(P);
}

static Expr* parse_primary(Parser *P) {
    Token t = P->lexer->cur;
    lex_next(P->lexer);

    if (t.type == TK_NUM) return ec_num(t.num);
    if (t.type == TK_VAR) return ec_vars(t.text);

    if (t.type == TK_LPAREN) {
        Expr *e = parse_expr(P);
        /* 期望 TK_RPAREN */
        if (P->lexer->cur.type == TK_RPAREN) lex_next(P->lexer);
        return e;
    }

    if (t.type == TK_CMD) {
        const char *cmd = t.text;

        /* \frac{a}{b} */
        if (strcmp(cmd, "\\frac") == 0) {
            Expr *num = parse_primary(P);
            Expr *den = parse_primary(P);
            return ec_binary(EC_DIV, num, den);
        }
        /* \sqrt{x} */
        if (strcmp(cmd, "\\sqrt") == 0) {
            Expr *arg = parse_primary(P);
            return ec_unary(EC_SQRT, arg);
        }
        /* \sin, \cos, \tan, ... */
        if (strcmp(cmd, "\\sin") == 0) return ec_unary(EC_SIN, parse_unary(P));
        if (strcmp(cmd, "\\cos") == 0) return ec_unary(EC_COS, parse_unary(P));
        if (strcmp(cmd, "\\tan") == 0) return ec_unary(EC_TAN, parse_unary(P));
        if (strcmp(cmd, "\\cot") == 0) return ec_unary(EC_COT, parse_unary(P));
        if (strcmp(cmd, "\\sec") == 0) return ec_unary(EC_SEC, parse_unary(P));
        if (strcmp(cmd, "\\csc") == 0) return ec_unary(EC_CSC, parse_unary(P));
        if (strcmp(cmd, "\\exp") == 0) return ec_unary(EC_EXP, parse_unary(P));
        if (strcmp(cmd, "\\ln") == 0) return ec_unary(EC_LN, parse_unary(P));
        if (strcmp(cmd, "\\log") == 0) return ec_unary(EC_LOG, parse_unary(P));
        if (strcmp(cmd, "\\log10") == 0) return ec_unary(EC_LOG10, parse_unary(P));
        if (strcmp(cmd, "\\log2") == 0) return ec_unary(EC_LOG2, parse_unary(P));
        if (strcmp(cmd, "\\sinh") == 0) return ec_unary(EC_SINH, parse_unary(P));
        if (strcmp(cmd, "\\cosh") == 0) return ec_unary(EC_COSH, parse_unary(P));
        if (strcmp(cmd, "\\tanh") == 0) return ec_unary(EC_TANH, parse_unary(P));
        if (strcmp(cmd, "\\abs") == 0) return ec_unary(EC_ABS, parse_unary(P));
        if (strcmp(cmd, "\\floor") == 0) return ec_unary(EC_FLOOR, parse_unary(P));
        if (strcmp(cmd, "\\ceil") == 0) return ec_unary(EC_CEIL, parse_unary(P));
        if (strcmp(cmd, "\\sign") == 0) return ec_unary(EC_SIGN, parse_unary(P));
        if (strcmp(cmd, "\\pi") == 0) return ec_const(EC_PI);
        if (strcmp(cmd, "\\e") == 0) return ec_const(EC_E);
        if (strcmp(cmd, "\\i") == 0) return ec_const(EC_I);
        if (strcmp(cmd, "\\infty") == 0) return ec_const(EC_INF);
        if (strcmp(cmd, "\\lim") == 0) return parse_unary(P); /* TODO */
        if (strcmp(cmd, "\\int") == 0) return parse_unary(P); /* TODO */
        if (strcmp(cmd, "\\sum") == 0) return parse_unary(P); /* TODO */
    }

    /* 空表达式 */
    return ec_num(0);
}

Expr* parser_parse(Parser *P) {
    Expr *e = parse_expr(P);
    if (P->lexer->cur.type != TK_EOF && P->lexer->cur.type != TK_ERR) {
        snprintf(P->err, sizeof(P->err), "意外的 token: %s", P->lexer->cur.text);
        P->err_pos = P->lexer->cur.pos;
    }
    return e;
}

Expr* parser_parse_op(Parser *P) { return parser_parse(P); }
int parser_error(const Parser *P) { return P->err[0] != 0; }
void parser_free(Parser *P) { (void)P; }

Expr* ec_parse(const char *latex) {
    parser_init(&g_parser, latex);
    Expr *e = parser_parse(&g_parser);
    return e;
}
void ec_parse_free(Expr *e) { ec_free(e); }
int ec_parse_error(void) { return parser_error(&g_parser); }
const char* ec_parse_error_msg(void) { return g_parser.err; }
int ec_parse_error_pos(void) { return g_parser.err_pos; }
