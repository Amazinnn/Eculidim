#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Derivation
 *============================================================*/
Derivation* ec_derivation_new(void) {
    Derivation *d = ec_malloc(sizeof(Derivation));
    d->steps = ec_malloc(16 * sizeof(DerivationStep));
    d->count = 0; d->capacity = 16;
    return d;
}

void ec_derivation_add(Derivation *d, const char *latex, const char *rule) {
    if (d->count >= d->capacity) {
        d->capacity *= 2;
        d->steps = ec_realloc(d->steps, d->capacity * sizeof(DerivationStep));
    }
    d->steps[d->count].step_latex = ec_strdup(latex);
    d->steps[d->count].rule = ec_strdup(rule);
    d->count++;
}

char* ec_derivation_render(const Derivation *d) {
    if (!d || d->count == 0) return ec_strdup("");
    size_t cap = 1024, len = 0;
    char *buf = ec_malloc(cap);
    for (int i = 0; i < d->count; i++) {
        int n = snprintf(buf + len, cap - len, "[%d] %s  (%s)\n",
                         i + 1, d->steps[i].step_latex, d->steps[i].rule);
        if (n < 0 || (size_t)n >= cap - len) break;
        len += n;
    }
    return buf;
}

char* ec_derivation_render_latex(const Derivation *d) {
    if (!d || d->count == 0) return ec_strdup("");
    size_t cap = 1024, len = 0;
    char *buf = ec_malloc(cap);
    len += snprintf(buf + len, cap - len, "\\begin{align*}\n");
    for (int i = 0; i < d->count; i++) {
        int n = snprintf(buf + len, cap - len, "& %s \\\\ %s \\\\\n",
                         d->steps[i].step_latex, d->steps[i].rule);
        if (n < 0 || (size_t)n >= cap - len) break;
        len += n;
    }
    len += snprintf(buf + len, cap - len, "\\end{align*}\n");
    return buf;
}

void ec_derivation_free(Derivation *d) {
    if (!d) return;
    for (int i = 0; i < d->count; i++) {
        ec_free(d->steps[i].step_latex);
        ec_free(d->steps[i].rule);
    }
    ec_free(d->steps); ec_free(d);
}

/*============================================================
 * LaTeX 命令表
 *============================================================*/
static const ECLatexCmd cmd_table[] = {
    {"sin", EC_SIN}, {"cos", EC_COS}, {"tan", EC_TAN},
    {"cot", EC_COT}, {"sec", EC_SEC}, {"csc", EC_CSC},
    {"arcsin", EC_ASIN}, {"arccos", EC_ACOS}, {"arctan", EC_ATAN},
    {"sinh", EC_SINH}, {"cosh", EC_COSH}, {"tanh", EC_TANH},
    {"exp", EC_EXP}, {"ln", EC_LN}, {"log", EC_LOG},
    {"sqrt", EC_SQRT}, {"abs", EC_ABS}, {"sign", EC_SIGN},
    {"floor", EC_FLOOR}, {"ceil", EC_CEIL}, {"pi", EC_PI},
    {"e", EC_E}, {"i", EC_I}, {"infty", EC_INF},
};

const ECLatexCmd* ec_latex_cmd_table(void) { return cmd_table; }
int ec_latex_cmd_count(void) { return sizeof(cmd_table) / sizeof(cmd_table[0]); }

ECNodeType ec_latex_cmd_lookup(const char *cmd) {
    for (int i = 0; i < ec_latex_cmd_count(); i++)
        if (strcmp(cmd + 1, cmd_table[i].latex) == 0) return cmd_table[i].type;
    return EC_NIL;
}

/*============================================================
 * LaTeX 输出
 *============================================================*/
static void latex_rec(const Expr *e, char *buf, size_t cap, size_t *pos) {
    if (!e) { snprintf(buf + *pos, cap - *pos, "0"); return; }
    switch (e->type) {
        case EC_NUM: *pos += snprintf(buf + *pos, cap - *pos, "%.10g", e->num_val); break;
        case EC_VAR:
            if (e->var_str) *pos += snprintf(buf + *pos, cap - *pos, "%s", e->var_str);
            else *pos += snprintf(buf + *pos, cap - *pos, "%c", e->var_name);
            break;
        case EC_ADD:
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, " + ");
            latex_rec(e->right, buf, cap, pos);
            break;
        case EC_SUB:
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, " - ");
            latex_rec(e->right, buf, cap, pos);
            break;
        case EC_MUL:
            *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, ") \\cdot (");
            latex_rec(e->right, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, ")");
            break;
        case EC_DIV:
            *pos += snprintf(buf + *pos, cap - *pos, "\\frac{");
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}{");
            latex_rec(e->right, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}");
            break;
        case EC_POW:
            *pos += snprintf(buf + *pos, cap - *pos, "{");
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}^{");
            latex_rec(e->right, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}");
            break;
        case EC_NEG:
            *pos += snprintf(buf + *pos, cap - *pos, "-");
            latex_rec(e->arg, buf, cap, pos);
            break;
        case EC_SQRT:
            *pos += snprintf(buf + *pos, cap - *pos, "\\sqrt{");
            latex_rec(e->arg, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}");
            break;
        case EC_SIN: case EC_COS: case EC_TAN: case EC_EXP: case EC_LN:
        case EC_SINH: case EC_COSH: case EC_TANH:
        case EC_ABS: case EC_FLOOR: case EC_CEIL: case EC_SIGN:
        case EC_COT: case EC_SEC: case EC_CSC:
        case EC_ASIN: case EC_ACOS: case EC_ATAN:
        case EC_ACOT: case EC_ASEC: case EC_ACSC:
        case EC_LOG: case EC_LOG10: case EC_LOG2:
        case EC_COTH: case EC_SECH: case EC_CSCH:
        case EC_ASINH: case EC_ACOSH: case EC_ATANH:
        case EC_EXP2: case EC_EXPM1: case EC_LN1P:
        case EC_ROUND: case EC_TRUNC: case EC_FDIM: case EC_MOD:
        case EC_LT: case EC_LE: case EC_GT: case EC_GE:
        case EC_EQ: case EC_NE: case EC_LAND: case EC_LOR: case EC_LNOT:
        case EC_REM: case EC_FACTORIAL: {
            static const char *names[] = {
                [EC_SIN]="sin",[EC_COS]="cos",[EC_TAN]="tan",
                [EC_COT]="cot",[EC_SEC]="sec",[EC_CSC]="csc",
                [EC_EXP]="exp",[EC_EXP2]="exp2",[EC_EXPM1]="expm1",
                [EC_LN]="ln",[EC_LN1P]="ln1p",
                [EC_LOG]="log",[EC_LOG10]="log_{10}",[EC_LOG2]="log_2",
                [EC_SINH]="sinh",[EC_COSH]="cosh",[EC_TANH]="tanh",
                [EC_COTH]="coth",[EC_SECH]="sech",[EC_CSCH]="csch",
                [EC_ASIN]="arcsin",[EC_ACOS]="arccos",[EC_ATAN]="arctan",
                [EC_ACOT]="arccot",[EC_ASEC]="arcsec",[EC_ACSC]="arccsc",
                [EC_ASINH]="arsinh",[EC_ACOSH]="arcosh",[EC_ATANH]="artanh",
                [EC_ABS]="abs",[EC_SIGN]="sign",
                [EC_FLOOR]="floor",[EC_CEIL]="ceil",
                [EC_ROUND]="round",[EC_TRUNC]="trunc",
                [EC_LT]="<",[EC_LE]="<=", [EC_GT]=">", [EC_GE]=">=",
                [EC_EQ]="=",  [EC_NE]="!=",
                [EC_LAND]="\\land",[EC_LOR]="\\lor",[EC_LNOT]="\\lnot",
                [EC_MOD]="\\bmod",[EC_REM]="\\rem",[EC_FDIM]="\\fdim",
                [EC_FACTORIAL]="!",
            };
            const char *n = names[e->type] ? names[e->type] : "\\text{op}";
            if (e->type == EC_LT || e->type == EC_LE || e->type == EC_GT ||
                e->type == EC_GE || e->type == EC_EQ || e->type == EC_NE ||
                e->type == EC_LAND || e->type == EC_LOR || e->type == EC_MOD ||
                e->type == EC_REM || e->type == EC_FDIM) {
                *pos += snprintf(buf + *pos, cap - *pos, "(");
                latex_rec(e->left, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, ")%s(", n);
                latex_rec(e->right, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, ")");
            } else if (e->type == EC_LNOT) {
                *pos += snprintf(buf + *pos, cap - *pos, "%s(", n);
                latex_rec(e->arg, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, ")");
            } else if (e->type == EC_FACTORIAL) {
                latex_rec(e->arg, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, "!");
            } else {
                *pos += snprintf(buf + *pos, cap - *pos, "\\%s(", n);
                latex_rec(e->arg, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, ")");
            }
            break;
        }
        case EC_PI: *pos += snprintf(buf + *pos, cap - *pos, "\\pi"); break;
        case EC_E: *pos += snprintf(buf + *pos, cap - *pos, "e"); break;
        case EC_I: *pos += snprintf(buf + *pos, cap - *pos, "i"); break;
        case EC_INF: *pos += snprintf(buf + *pos, cap - *pos, "\\infty"); break;
        case EC_INT:
            if (e->left && e->right) {
                *pos += snprintf(buf + *pos, cap - *pos, "\\int_{");
                latex_rec(e->left, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, "}^{");
                latex_rec(e->right, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, "} ");
                latex_rec(e->arg, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, " \\, d%c", e->deriv_var);
            } else {
                *pos += snprintf(buf + *pos, cap - *pos, "\\int ");
                latex_rec(e->arg, buf, cap, pos);
                *pos += snprintf(buf + *pos, cap - *pos, " \\, d%c", e->deriv_var);
            }
            break;
        case EC_DERIV:
            *pos += snprintf(buf + *pos, cap - *pos, "\\frac{d^{%d}}{d%c^{%d}} ",
                             e->deriv_order, e->deriv_var, e->deriv_order);
            latex_rec(e->arg, buf, cap, pos);
            break;
        case EC_LIMIT:
            *pos += snprintf(buf + *pos, cap - *pos, "\\lim_{%c \\to ", e->deriv_var);
            latex_rec(e->cond, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "} ");
            latex_rec(e->arg, buf, cap, pos);
            break;
        case EC_SUM:
            *pos += snprintf(buf + *pos, cap - *pos, "\\sum_{%c=", e->deriv_var);
            latex_rec(e->left, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "}^{");
            latex_rec(e->right, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, "} ");
            latex_rec(e->arg, buf, cap, pos);
            break;
        case EC_SERIES:
            *pos += snprintf(buf + *pos, cap - *pos, "\\text{Taylor}[");
            latex_rec(e->arg, buf, cap, pos);
            *pos += snprintf(buf + *pos, cap - *pos, ", %d]", e->deriv_order);
            break;
        default:
            *pos += snprintf(buf + *pos, cap - *pos, "\\text{op:%d}", e->type);
            break;
    }
}

char* ec_to_latex(const Expr *e) {
    char *buf = ec_malloc(4096); buf[0] = 0;
    size_t pos = 0; latex_rec(e, buf, 4096, &pos);
    return buf;
}

char* ec_to_latex_pretty(const Expr *e) { return ec_to_latex(e); }

char* ec_to_latex_inline(const Expr *e) {
    char *s = ec_to_latex(e);
    if (!s) return ec_strdup("\\( \\)");
    size_t len = strlen(s);
    char *r = ec_malloc(len + 7);
    snprintf(r, len + 7, "\\(%s\\)", s);
    ec_free(s);
    return r;
}

/* ASCII tree representation of AST (for debugging) */
static void tree_node_name(char *buf, size_t cap, const Expr *e) {
    switch (e->type) {
        case EC_NUM:  snprintf(buf, cap, "NUM:%.10g", e->num_val); break;
        case EC_VAR:  snprintf(buf, cap, "VAR:%s", e->var_str ? e->var_str : "x"); break;
        case EC_PI:   snprintf(buf, cap, "PI"); break;
        case EC_E:    snprintf(buf, cap, "E"); break;
        case EC_I:    snprintf(buf, cap, "I"); break;
        case EC_INF:  snprintf(buf, cap, "INF"); break;
        case EC_ADD:  snprintf(buf, cap, "ADD"); break;
        case EC_SUB:  snprintf(buf, cap, "SUB"); break;
        case EC_MUL:  snprintf(buf, cap, "MUL"); break;
        case EC_DIV:  snprintf(buf, cap, "DIV"); break;
        case EC_POW:  snprintf(buf, cap, "POW"); break;
        case EC_NEG:  snprintf(buf, cap, "NEG"); break;
        case EC_MOD:  snprintf(buf, cap, "MOD"); break;
        case EC_SIN:  snprintf(buf, cap, "SIN"); break;
        case EC_COS:  snprintf(buf, cap, "COS"); break;
        case EC_TAN:  snprintf(buf, cap, "TAN"); break;
        case EC_SQRT: snprintf(buf, cap, "SQRT"); break;
        case EC_EXP:  snprintf(buf, cap, "EXP"); break;
        case EC_LN:   snprintf(buf, cap, "LN"); break;
        case EC_INT:  snprintf(buf, cap, "INT"); break;
        case EC_DERIV:snprintf(buf, cap, "DERIV"); break;
        case EC_LIMIT:snprintf(buf, cap, "LIMIT"); break;
        case EC_SUM:  snprintf(buf, cap, "SUM"); break;
        case EC_SERIES:snprintf(buf, cap, "SERIES"); break;
        default:      snprintf(buf, cap, "OP:%d", e->type); break;
    }
}

static void tree_rec(const Expr *e, char *buf, size_t cap,
                     size_t *pos, const char *prefix, int is_last) {
    if (!e) return;
    char name[32];
    tree_node_name(name, sizeof(name), e);
    int n = snprintf(buf + *pos, cap - *pos, "%s[%s]\n", prefix, name);
    if (n < 0 || (size_t)n >= cap - *pos) return;
    *pos += n;

    /* build child prefix */
    char child_pre[256];
    snprintf(child_pre, sizeof(child_pre), "%s%s",
             prefix, is_last ? "    " : "|   ");

    if (e->left) {
        n = snprintf(buf + *pos, cap - *pos, "%s|-- L: ", prefix);
        if (n < 0 || (size_t)n >= cap - *pos) return;
        *pos += n;
        tree_rec(e->left, buf, cap, pos, child_pre, !e->right && !e->arg);
    }
    if (e->right) {
        n = snprintf(buf + *pos, cap - *pos, "%s|-- R: ", prefix);
        if (n < 0 || (size_t)n >= cap - *pos) return;
        *pos += n;
        tree_rec(e->right, buf, cap, pos, child_pre, !e->arg);
    }
    if (e->arg) {
        n = snprintf(buf + *pos, cap - *pos, "%s\\-- A: ", prefix);
        if (n < 0 || (size_t)n >= cap - *pos) return;
        *pos += n;
        tree_rec(e->arg, buf, cap, pos, child_pre, 1);
    }
    if (e->cond) {
        n = snprintf(buf + *pos, cap - *pos, "%s\\-- C: ", prefix);
        if (n < 0 || (size_t)n >= cap - *pos) return;
        *pos += n;
        tree_rec(e->cond, buf, cap, pos, child_pre, 1);
    }
}

char* ec_to_latex_tree(const Expr *e) {
    char *buf = ec_malloc(8192);
    buf[0] = '\0';
    size_t pos = 0;
    if (!e) { snprintf(buf, 256, "(null)\n"); return buf; }
    tree_rec(e, buf, 8192, &pos, "", 1);
    return buf;
}

char* ec_to_latex_html(const Expr *e) {
    char *s = ec_to_latex(e);
    if (!s) return ec_strdup("\\(<empty>\\)");
    size_t len = strlen(s);
    char *r = ec_malloc(len + 7);
    snprintf(r, len + 7, "\\(%s\\)", s);
    ec_free(s);
    return r;
}
void ec_latex_free(char *s) { ec_free(s); }
char* ec_latex_normalize(const char *latex) { return ec_strdup(latex); }
