#include "eculid.h"

static Expr* diff_rec(const Expr *e, char var) {
    if (!e) return NULL;
    switch (e->type) {
        case EC_NUM: return ec_num(0);
        case EC_VAR:
            if (e->var_name == var) return ec_num(1);
            return ec_num(0);
        case EC_ADD:
            return ec_binary(EC_ADD, diff_rec(e->left, var), diff_rec(e->right, var));
        case EC_SUB:
            return ec_binary(EC_SUB, diff_rec(e->left, var), diff_rec(e->right, var));
        case EC_MUL:
            return ec_binary(EC_ADD,
                ec_binary(EC_MUL, diff_rec(e->left, var), ec_copy(e->right)),
                ec_binary(EC_MUL, ec_copy(e->left), diff_rec(e->right, var)));
        case EC_DIV: {
            Expr *u = e->left, *v = e->right;
            return ec_binary(EC_DIV,
                ec_binary(EC_SUB,
                    ec_binary(EC_MUL, diff_rec(u, var), ec_copy(v)),
                    ec_binary(EC_MUL, ec_copy(u), diff_rec(v, var))),
                ec_binary(EC_POW, ec_copy(v), ec_num(2)));
        }
        case EC_POW: {
            Expr *base = e->left, *exp = e->right;
            if (ec_is_const(exp))
                return ec_binary(EC_MUL,
                    ec_binary(EC_MUL, ec_copy(exp), ec_binary(EC_POW, ec_copy(base), ec_num(exp->num_val - 1))),
                    diff_rec(base, var));
            if (ec_is_one(base))
                return ec_num(0);
            return ec_binary(EC_MUL,
                ec_copy(e),
                ec_binary(EC_ADD,
                    ec_binary(EC_DIV, diff_rec(exp, var), ec_copy(exp)),
                    ec_binary(EC_DIV,
                        ec_binary(EC_MUL, diff_rec(base, var), ec_copy(exp)),
                        ec_copy(base))));
        }
        case EC_NEG:
            return ec_unary(EC_NEG, diff_rec(e->arg, var));
        case EC_SIN:
            return ec_binary(EC_MUL, ec_unary(EC_COS, ec_copy(e->arg)), diff_rec(e->arg, var));
        case EC_COS:
            return ec_binary(EC_MUL, ec_unary(EC_NEG, ec_unary(EC_SIN, ec_copy(e->arg))), diff_rec(e->arg, var));
        case EC_TAN:
            return ec_binary(EC_DIV, diff_rec(e->arg, var), ec_binary(EC_POW, ec_unary(EC_COS, ec_copy(e->arg)), ec_num(2)));
        case EC_EXP:
            return ec_binary(EC_MUL, ec_copy(e), diff_rec(e->arg, var));
        case EC_LN: {
            Expr *u = e->arg;
            return ec_binary(EC_DIV, diff_rec(u, var), ec_copy(u));
        }
        case EC_SQRT: {
            Expr *u = e->arg;
            return ec_binary(EC_DIV, diff_rec(u, var), ec_binary(EC_MUL, ec_num(2), ec_copy(e)));
        }
        default:
            return ec_copy(e);
    }
}

Expr* ec_diff(Expr *e, char var) { return diff_rec(e, var); }

Expr* ec_diff_n(Expr *e, char var, int n) {
    Expr *r = ec_copy(e);
    for (int i = 0; i < n; i++) {
        Expr *next = diff_rec(r, var);
        ec_free_expr(r); r = next;
    }
    return r;
}

Expr* ec_partial_diff(Expr *e, char var) { return diff_rec(e, var); }

Expr* ec_total_diff(Expr *e, const char **vars, int n) {
    (void)vars; (void)n; return e ? ec_copy(e) : NULL;
}

Derivation* ec_diff_with_steps(Expr *e, char var) {
    (void)e; (void)var; return NULL; /* TODO */
}

char* ec_diff_rule_name(ECNodeType t) {
    switch (t) {
        case EC_ADD: return "加法法则";
        case EC_SUB: return "减法法则";
        case EC_MUL: return "乘法法则";
        case EC_DIV: return "除法法则";
        case EC_POW: return "幂函数求导";
        case EC_SIN: return "sin求导";
        case EC_COS: return "cos求导";
        case EC_EXP: return "指数求导";
        case EC_LN: return "对数求导";
        default: return "未知规则";
    }
}
