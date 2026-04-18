#include "eculid.h"
#include <math.h>

static Expr* integ_rec(const Expr *e, char var) {
    if (!e) return NULL;
    switch (e->type) {
        case EC_NUM: return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));
        case EC_VAR:
            if (e->var_name == var)
                return ec_binary(EC_DIV, ec_binary(EC_POW, ec_varc(var), ec_num(2)), ec_num(2));
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));
        case EC_ADD:
            return ec_binary(EC_ADD, integ_rec(e->left, var), integ_rec(e->right, var));
        case EC_SUB:
            return ec_binary(EC_SUB, integ_rec(e->left, var), integ_rec(e->right, var));
        case EC_NEG:
            return ec_unary(EC_NEG, integ_rec(e->arg, var));
        case EC_POW: {
            if (e->left->type == EC_VAR && e->left->var_name == var && e->right->type == EC_NUM) {
                double n = e->right->num_val;
                if (n == -1) return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
                double coef = 1.0 / (n + 1);
                return ec_binary(EC_MUL, ec_num(coef), ec_binary(EC_POW, ec_varc(var), ec_num(n + 1)));
            }
            break;
        }
        case EC_SIN:
            if (e->arg->type == EC_VAR && e->arg->var_name == var)
                return ec_unary(EC_NEG, ec_cos(ec_varc(var)));
            break;
        case EC_COS:
            if (e->arg->type == EC_VAR && e->arg->var_name == var)
                return ec_sin(ec_varc(var));
            break;
        case EC_EXP:
            if (e->arg->type == EC_VAR && e->arg->var_name == var)
                return ec_copy(e);
            break;
        default:
            break;
    }
    return ec_intg(ec_copy(e), var); /* 无法积分，返回带积分符号的节点 */
}

Expr* ec_integrate(Expr *e, char var) { return integ_rec(e, var); }

Expr* ec_definite_integral(Expr *e, char var, Expr *a, Expr *b) {
    Expr *F = integ_rec(e, var);
    /* TODO: 代入上下限定理 — 目前返回不定积分加标记 */
    Expr *node = ec_defintg(e, var, a, b);
    (void)F;
    return node;
}

int ec_has_integral(Expr *e, char var) {
    (void)e; (void)var; return 0;
}

Derivation* ec_integrate_with_steps(Expr *e, char var) {
    (void)e; (void)var; return NULL; /* TODO */
}

static double apply_env(const Expr *e, char var, double val, const ECEnv *base) {
    (void)e; (void)var; (void)val; (void)base; return 0.0;
}

double ec_numerical_integral(Expr *e, char var, double a, double b, const char *method) {
    (void)e; (void)var; (void)a; (void)b; (void)method; return 0.0; /* TODO */
}

double ec_numerical_integral_ex(Expr *e, char var, double a, double b,
                                  ECIntMethod method, double tol) {
    (void)e; (void)var; (void)a; (void)b; (void)method; (void)tol; return 0.0; /* TODO */
}
