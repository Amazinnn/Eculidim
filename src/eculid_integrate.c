#include "eculid.h"
#include "eculid_series.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Internal helpers
 *============================================================*/
static int expr_is_simple_var(const Expr *e, char var) {
    return e && e->type == EC_VAR && e->var_name == var && e->var_str == NULL;
}

static int expr_var_matches(const Expr *e, char var) {
    return e && e->type == EC_VAR && e->var_name == var;
}

static int node_has_integral(const Expr *e, char var);
static Expr* integ_rec(const Expr *e, char var);
static Expr* integ_trig_power(const Expr *e, char var);
static Expr* integ_exp_trig(const Expr *e, char var);
static Expr* integ_tabular(const Expr *e, char var);

static int integ_tabular_hit_limit = 0;

static int sub_has_integral(const Expr *e, char var) {
    return node_has_integral(e, var);
}

static int node_has_integral(const Expr *e, char var) {
    (void)var;
    if (!e) return 0;
    switch (e->type) {
        case EC_NUM: case EC_VAR: case EC_PI: case EC_E: case EC_I:
        case EC_INF: case EC_NINF:
            return 1;
        case EC_ADD: case EC_SUB:
            return sub_has_integral(e->left, var) && sub_has_integral(e->right, var);
        case EC_MUL: {
            if (ec_is_const(e->left)) return sub_has_integral(e->right, var);
            if (ec_is_const(e->right)) return sub_has_integral(e->left, var);
            return sub_has_integral(e->left, var) && sub_has_integral(e->right, var);
        }
        case EC_DIV: {
            return sub_has_integral(e->left, var) && sub_has_integral(e->right, var);
        }
        case EC_NEG:
            return sub_has_integral(e->arg, var);
        case EC_POW: {
            if (e->left && e->left->type == EC_VAR && e->left->var_name == var) return 1;
            if (e->right && e->right->type == EC_VAR && e->right->var_name == var) return 1;
            return 1;
        }
        case EC_SIN: case EC_COS: case EC_TAN: case EC_COT:
        case EC_SEC: case EC_CSC:
        case EC_SINH: case EC_COSH: case EC_TANH: case EC_COTH:
        case EC_SECH: case EC_CSCH:
        case EC_EXP:
        case EC_LN: case EC_LOG: case EC_LOG10: case EC_LOG2:
        case EC_SQRT: case EC_CBRT:
        case EC_ASIN: case EC_ACOS: case EC_ATAN:
        case EC_ACOT: case EC_ASEC: case EC_ACSC:
        case EC_ASINH: case EC_ACOSH: case EC_ATANH:
        case EC_ACOTH: case EC_ASECH: case EC_ACSCH:
            return 1;
        default:
            return 0;
    }
}

int ec_has_integral(Expr *e, char var) {
    return node_has_integral(e, var);
}

/*============================================================
 * Integration by parts helpers
 *============================================================*/
static Expr* integ_by_parts(const Expr *e, char var) {
    if (e->type != EC_MUL) return NULL;

    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    if (B->type == EC_POW &&
        B->right && B->right->type == EC_NUM && B->right->num_val >= 0 &&
        B->left && B->left->type == EC_VAR && B->left->var_name == var) {
        A = e->right; B = e->left;
    }

    int poly_deg = -1;
    if (A->type == EC_VAR && A->var_name == var && !A->var_str)
        poly_deg = 1;
    else if (A->type == EC_POW &&
             A->left && A->left->type == EC_VAR && A->left->var_name == var &&
             A->right && A->right->type == EC_NUM && A->right->num_val >= 0 &&
             !A->left->var_str)
        poly_deg = (int)(A->right->num_val + 0.5);
    else if (A->type == EC_NUM)
        poly_deg = 0;

    if (poly_deg < 0) return NULL;

    Expr *u, *du, *v;
    if (poly_deg == 0) {
        return NULL;
    } else {
        u = ec_copy(A);
        du = ec_diff(ec_copy(A), var);
        Expr *IB = integ_rec(B, var);
        if (!IB || IB->type == EC_INT) {
            ec_free_expr(u);
            ec_free_expr(du);
            ec_free_expr(IB);
            return NULL;
        }
        v = IB;
    }

    Expr *uv = ec_binary(EC_MUL, u, ec_copy(v));
    Expr *ivdu = integ_rec(ec_binary(EC_MUL, ec_copy(v), du), var);
    Expr *result;
    if (ivdu && ivdu->type != EC_INT) {
        result = ec_binary(EC_SUB, uv, ivdu);
    } else {
        ec_free_expr(ivdu);
        ec_free_expr(uv);
        ec_free_expr(v);
        return NULL;
    }
    ec_free_expr(v);
    return ec_simplify(result);
}

/*============================================================
 * Rational function antiderivatives
 *============================================================*/
static Expr* try_rational_antideriv(const Expr *e, char var) {
    if (e->type == EC_DIV) {
        const Expr *num = e->left;
        const Expr *den = e->right;
        if (num && den &&
            num->type == EC_NUM && num->num_val == 1.0 &&
            den->type == EC_ADD) {
            const Expr *term1 = den->left;
            const Expr *term2 = den->right;
            if (term1 && term1->type == EC_POW &&
                term1->left && term1->left->type == EC_VAR && term1->left->var_name == var &&
                term1->right && term1->right->type == EC_NUM && term1->right->num_val == 2.0 &&
                term2 && term2->type == EC_NUM && term2->num_val == 1.0) {
                return ec_unary(EC_ATAN, ec_varc(var));
            }
        }
        if (num && den &&
            num->type == EC_NUM && num->num_val == 1.0 &&
            den->type == EC_VAR && den->var_name == var) {
            return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
        }
    }
    return NULL;
}

/*============================================================
 * Trigonometric power-reduction formulas
 *============================================================*/
static Expr* integ_trig_power(const Expr *e, char var) {
    if (!e || e->type != EC_POW) return NULL;
    const Expr *base = e->left;
    const Expr *exp_node = e->right;
    if (!base || !exp_node || exp_node->type != EC_NUM) return NULL;
    if ((base->type != EC_SIN && base->type != EC_COS) || !base->arg) return NULL;
    if (!expr_is_simple_var(base->arg, var)) return NULL;
    double n = exp_node->num_val;
    if (n < 2.0) return NULL;

    if (base->type == EC_SIN) {
        if (fabs(n - 2.0) < 1e-12) {
            Expr *term1 = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
            Expr *two_x = ec_binary(EC_MUL, ec_num(2), ec_varc(var));
            Expr *sin_2x = ec_unary(EC_SIN, two_x);
            Expr *term2 = ec_binary(EC_DIV, sin_2x, ec_num(4));
            return ec_simplify(ec_binary(EC_SUB, term1, term2));
        }
        Expr *sn_1 = ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n - 1));
        Expr *cos_x = ec_unary(EC_COS, ec_varc(var));
        Expr *term1 = ec_binary(EC_DIV, ec_binary(EC_MUL, ec_unary(EC_NEG, sn_1), cos_x), ec_num(n));

        Expr *next = ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n - 2));
        Expr *r2 = integ_trig_power(next, var);
        if (!r2) r2 = integ_rec(next, var);
        Expr *term2 = ec_binary(EC_MUL, ec_num((n - 1.0) / n), r2);
        return ec_simplify(ec_binary(EC_ADD, term1, term2));
    }

    if (base->type == EC_COS) {
        if (fabs(n - 2.0) < 1e-12) {
            Expr *term1 = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
            Expr *two_x = ec_binary(EC_MUL, ec_num(2), ec_varc(var));
            Expr *sin_2x = ec_unary(EC_SIN, two_x);
            Expr *term2 = ec_binary(EC_DIV, sin_2x, ec_num(4));
            return ec_simplify(ec_binary(EC_ADD, term1, term2));
        }
        Expr *cn_1 = ec_binary(EC_POW, ec_unary(EC_COS, ec_varc(var)), ec_num(n - 1));
        Expr *sin_x = ec_unary(EC_SIN, ec_varc(var));
        Expr *term1 = ec_binary(EC_DIV, ec_binary(EC_MUL, cn_1, sin_x), ec_num(n));

        Expr *next = ec_binary(EC_POW, ec_unary(EC_COS, ec_varc(var)), ec_num(n - 2));
        Expr *r2 = integ_trig_power(next, var);
        if (!r2) r2 = integ_rec(next, var);
        Expr *term2 = ec_binary(EC_MUL, ec_num((n - 1.0) / n), r2);
        return ec_simplify(ec_binary(EC_ADD, term1, term2));
    }

    return NULL;
}

static int is_var_mul_num(const Expr *arg, char var, double *k) {
    if (!arg) return 0;
    if (arg->type == EC_VAR && arg->var_name == var) {
        *k = 1.0;
        return 1;
    }
    if (arg->type == EC_MUL && arg->left && arg->right) {
        if (arg->left->type == EC_NUM && arg->right->type == EC_VAR && arg->right->var_name == var) {
            *k = arg->left->num_val;
            return 1;
        }
        if (arg->right->type == EC_NUM && arg->left->type == EC_VAR && arg->left->var_name == var) {
            *k = arg->right->num_val;
            return 1;
        }
    }
    return 0;
}

static int is_e_pow_ax(const Expr *node, char var, double *a) {
    if (!node || node->type != EC_POW || !node->left || !node->right) return 0;
    if (node->left->type != EC_E) return 0;
    return is_var_mul_num(node->right, var, a);
}

/* Detect and integrate e^(ax) * sin(bx) / cos(bx) */
static Expr* integ_exp_trig(const Expr *e, char var) {
    if (!e || e->type != EC_MUL) return NULL;
    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    const Expr *exp_part = NULL;
    const Expr *trig_part = NULL;
    double dummy = 0.0;

    int a_is_exp = is_e_pow_ax(A, var, &dummy);
    int b_is_exp = is_e_pow_ax(B, var, &dummy);

    if (a_is_exp && (B->type == EC_SIN || B->type == EC_COS)) {
        exp_part = A; trig_part = B;
    } else if (b_is_exp && (A->type == EC_SIN || A->type == EC_COS)) {
        exp_part = B; trig_part = A;
    } else {
        return NULL;
    }

    double a = 0.0, b = 0.0;
    if (!is_e_pow_ax(exp_part, var, &a)) return NULL;
    if (!trig_part->arg) return NULL;
    if (!is_var_mul_num(trig_part->arg, var, &b)) return NULL;

    double denom = a * a + b * b;
    if (fabs(denom) < 1e-15) return NULL;

    Expr *exp_ax = ec_copy(exp_part);
    Expr *bx = (fabs(b - 1.0) < 1e-12) ? ec_varc(var) : ec_binary(EC_MUL, ec_num(b), ec_varc(var));
    Expr *sin_bx = ec_unary(EC_SIN, ec_copy(bx));
    Expr *cos_bx = ec_unary(EC_COS, bx);

    Expr *combo = NULL;
    if (trig_part->type == EC_SIN) {
        Expr *a_sin = ec_binary(EC_MUL, ec_num(a), sin_bx);
        Expr *b_cos = ec_binary(EC_MUL, ec_num(b), cos_bx);
        combo = ec_binary(EC_SUB, a_sin, b_cos);
    } else {
        Expr *a_cos = ec_binary(EC_MUL, ec_num(a), cos_bx);
        Expr *b_sin = ec_binary(EC_MUL, ec_num(b), sin_bx);
        combo = ec_binary(EC_ADD, a_cos, b_sin);
    }

    Expr *num = ec_binary(EC_MUL, exp_ax, combo);
    return ec_simplify(ec_binary(EC_DIV, num, ec_num(denom)));
}

static int poly_degree_if_simple(const Expr *p, char var) {
    if (!p) return -1;
    if (p->type == EC_VAR && p->var_name == var && !p->var_str) return 1;
    if (p->type == EC_POW && p->left && p->left->type == EC_VAR && p->left->var_name == var &&
        p->right && p->right->type == EC_NUM) {
        double n = p->right->num_val;
        if (n >= 0 && fabs(n - floor(n + 0.5)) < 1e-9) return (int)(n + 0.5);
    }
    if (p->type == EC_NUM) return 0;
    return -1;
}

/* Tabular (DI) method for ∫P(x)·T(x)dx where T in {sin(kx), cos(kx), e^(kx)} */
static Expr* integ_tabular(const Expr *e, char var) {
    if (!e || e->type != EC_MUL) return NULL;
    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    const Expr *poly = A;
    const Expr *trans = B;
    int deg = poly_degree_if_simple(poly, var);
    if (deg < 1) {
        poly = B;
        trans = A;
        deg = poly_degree_if_simple(poly, var);
    }
    if (deg < 1) return NULL;

    if (!(trans->type == EC_SIN || trans->type == EC_COS || trans->type == EC_POW)) return NULL;

    double k = 1.0;
    if (trans->type == EC_POW) {
        if (!is_e_pow_ax(trans, var, &k)) return NULL;
    } else {
        if (!is_var_mul_num(trans->arg, var, &k)) return NULL;
    }
    if (fabs(k) < 1e-15) return NULL;

    const int MAX_ITER = 20;
    int sign = 1;
    Expr *result = NULL;
    Expr *dcur = ec_copy(poly);

    for (int i = 0; i < MAX_ITER; i++) {
        if (dcur->type == EC_NUM && fabs(dcur->num_val) < 1e-14) break;

        Expr *ik = NULL;
        Expr *kx = (fabs(k - 1.0) < 1e-12) ? ec_varc(var) : ec_binary(EC_MUL, ec_num(k), ec_varc(var));
        if (trans->type == EC_POW) {
            ik = ec_binary(EC_DIV, ec_binary(EC_POW, ec_const(EC_E), ec_copy(kx)), ec_num(pow(k, i + 1)));
        } else if (trans->type == EC_SIN) {
            if ((i % 2) == 0) ik = ec_binary(EC_DIV, ec_unary(EC_NEG, ec_unary(EC_COS, ec_copy(kx))), ec_num(pow(k, i + 1)));
            else ik = ec_binary(EC_DIV, ec_unary(EC_SIN, ec_copy(kx)), ec_num(pow(k, i + 1)));
        } else {
            if ((i % 2) == 0) ik = ec_binary(EC_DIV, ec_unary(EC_SIN, ec_copy(kx)), ec_num(pow(k, i + 1)));
            else ik = ec_binary(EC_DIV, ec_unary(EC_COS, ec_copy(kx)), ec_num(pow(k, i + 1)));
        }
        ec_free_expr(kx);

        Expr *term = ec_binary(EC_MUL, ec_copy(dcur), ik);
        if (sign < 0) term = ec_unary(EC_NEG, term);

        if (!result) result = term;
        else result = ec_binary(EC_ADD, result, term);

        Expr *next = ec_diff(dcur, var);
        ec_free_expr(dcur);
        dcur = next;
        sign = -sign;
    }

    if (dcur && !(dcur->type == EC_NUM && fabs(dcur->num_val) < 1e-14)) {
        integ_tabular_hit_limit = 1;
    }

    ec_free_expr(dcur);
    if (!result) return NULL;
    return ec_simplify(result);
}

/*============================================================
 * Internal recursive integrator
 *============================================================*/
static Expr* integ_rec(const Expr *e, char var) {
    if (!e) return NULL;

    if (e->type == EC_INT)
        return integ_rec(e->arg, e->deriv_var ? e->deriv_var : var);

    switch (e->type) {
        case EC_NUM:
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        case EC_VAR:
            if (e->var_name == var)
                return ec_binary(EC_DIV, ec_binary(EC_POW, ec_varc(var), ec_num(2)), ec_num(2));
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        case EC_PI: case EC_E: case EC_I:
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        case EC_ADD:
            return ec_binary(EC_ADD, integ_rec(e->left, var), integ_rec(e->right, var));

        case EC_SUB:
            return ec_binary(EC_SUB, integ_rec(e->left, var), integ_rec(e->right, var));

        case EC_NEG:
            return ec_unary(EC_NEG, integ_rec(e->arg, var));

        case EC_POW: {
            double n = 0.0;
            int have_num = 0;
            if (e->right && e->right->type == EC_NUM) {
                n = e->right->num_val; have_num = 1;
            } else if (e->right && e->right->type == EC_NEG &&
                       e->right->arg && e->right->arg->type == EC_NUM) {
                n = -e->right->arg->num_val; have_num = 1;
            }
            if (e->left && e->left->type == EC_VAR && e->left->var_name == var) {
                if (have_num) {
                    if (n == -1) {
                        return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
                    }
                    return ec_binary(EC_MUL, ec_num(1.0 / (n + 1)),
                                      ec_binary(EC_POW, ec_varc(var), ec_num(n + 1)));
                }
                if (e->right && e->right->type == EC_VAR && e->right->var_name == var) {
                    Expr *ln_a = ec_unary(EC_LN, ec_copy(e->left));
                    return ec_binary(EC_DIV, ec_copy(e), ln_a);
                }
            }
            if (have_num && n >= 2) {
                Expr *r = integ_trig_power(e, var);
                if (r) return r;
            }
            if (have_num) {
                if (n == -1) {
                    return ec_unary(EC_LN, ec_unary(EC_ABS, ec_copy(e->left)));
                }
                Expr *chain_term = ec_diff(e->left, var);
                Expr *den = ec_binary(EC_MUL, ec_num(n + 1), chain_term);
                Expr *result = ec_binary(EC_DIV,
                    ec_binary(EC_POW, ec_copy(e->left), ec_num(n + 1)),
                    den);
                return result;
            }
            break;
        }

        case EC_SIN:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_NEG, ec_unary(EC_COS, ec_varc(var)));
            { Expr *du = ec_diff(e->arg, var);
              Expr *F = ec_binary(EC_DIV,
                  ec_unary(EC_NEG, ec_unary(EC_COS, ec_copy(e->arg))),
                  ec_copy(du));
              return ec_simplify(F); }
        case EC_COS:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_SIN, ec_varc(var));
            { Expr *du = ec_diff(e->arg, var);
              Expr *F = ec_binary(EC_DIV,
                  ec_unary(EC_SIN, ec_copy(e->arg)),
                  ec_copy(du));
              return ec_simplify(F); }
        case EC_TAN:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_NEG, ec_unary(EC_LN, ec_unary(EC_COS, ec_varc(var))));
            break;
        case EC_COT:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_LN, ec_unary(EC_SIN, ec_varc(var)));
            break;
        case EC_SEC:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *sec_x = ec_unary(EC_SEC, ec_varc(var));
                Expr *tan_x = ec_unary(EC_TAN, ec_varc(var));
                return ec_unary(EC_LN, ec_binary(EC_ADD, sec_x, tan_x));
            }
            break;
        case EC_CSC:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *csc_x = ec_unary(EC_CSC, ec_varc(var));
                Expr *cot_x = ec_unary(EC_COT, ec_varc(var));
                return ec_unary(EC_NEG, ec_unary(EC_LN, ec_binary(EC_ADD, csc_x, cot_x)));
            }
            break;

        case EC_EXP:
            if (expr_is_simple_var(e->arg, var))
                return ec_copy(e);
            { Expr *du = ec_diff(e->arg, var);
              return ec_binary(EC_DIV, ec_copy(e), ec_copy(du)); }
            break;

        case EC_LN:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *ln_x = ec_unary(EC_LN, ec_varc(var));
                return ec_binary(EC_SUB, ec_binary(EC_MUL, ec_varc(var), ln_x), ec_varc(var));
            }
            break;
        case EC_LOG:
        case EC_LOG10:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *ln10 = ec_unary(EC_LN, ec_const(EC_E));
                Expr *log_x = ec_unary(e->type, ec_varc(var));
                return ec_binary(EC_SUB, ec_binary(EC_MUL, ec_varc(var), log_x),
                                  ec_binary(EC_DIV, ec_varc(var), ln10));
            }
            break;
        case EC_LOG2:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *ln2 = ec_unary(EC_LN, ec_num(2));
                Expr *log2_x = ec_unary(EC_LOG2, ec_varc(var));
                return ec_binary(EC_SUB, ec_binary(EC_MUL, ec_varc(var), log2_x),
                                  ec_binary(EC_DIV, ec_varc(var), ln2));
            }
            break;

        case EC_SQRT:
            if (expr_is_simple_var(e->arg, var)) {
                return ec_binary(EC_MUL, ec_num(2.0/3.0),
                                  ec_binary(EC_POW, ec_varc(var), ec_num(1.5)));
            }
            { Expr *du = ec_diff(e->arg, var);
              Expr *F = ec_binary(EC_MUL, ec_num(2.0),
                      ec_binary(EC_DIV,
                          ec_binary(EC_POW, ec_copy(e->arg), ec_num(0.5)),
                          ec_copy(du)));
              return ec_simplify(F); }
        case EC_CBRT:
            if (expr_is_simple_var(e->arg, var)) {
                return ec_binary(EC_MUL, ec_num(0.75),
                                  ec_binary(EC_POW, ec_varc(var), ec_num(4.0/3.0)));
            }
            break;

        case EC_ASIN:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_asin = ec_binary(EC_MUL, ec_varc(var),
                                          ec_unary(EC_ASIN, ec_varc(var)));
                Expr *sgn = ec_unary(EC_SQRT,
                              ec_binary(EC_SUB, ec_num(1),
                                  ec_binary(EC_POW, ec_varc(var), ec_num(2))));
                return ec_binary(EC_ADD, x_asin, sgn);
            }
            break;
        case EC_ACOS:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_acos = ec_binary(EC_MUL, ec_varc(var),
                                          ec_unary(EC_ACOS, ec_varc(var)));
                Expr *sgn = ec_unary(EC_NEG,
                              ec_unary(EC_SQRT,
                                  ec_binary(EC_SUB, ec_num(1),
                                      ec_binary(EC_POW, ec_varc(var), ec_num(2)))));
                return ec_binary(EC_SUB, x_acos, sgn);
            }
            break;
        case EC_ATAN:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_atan = ec_binary(EC_MUL, ec_varc(var),
                                          ec_unary(EC_ATAN, ec_varc(var)));
                Expr *ln_term = ec_binary(EC_MUL, ec_num(0.5),
                              ec_unary(EC_LN,
                                  ec_binary(EC_ADD, ec_num(1),
                                      ec_binary(EC_POW, ec_varc(var), ec_num(2)))));
                return ec_binary(EC_SUB, x_atan, ln_term);
            }
            break;

        case EC_SINH:
            if (expr_var_matches(e->arg, var))
                return ec_unary(EC_COSH, ec_varc(var));
            break;
        case EC_COSH:
            if (expr_var_matches(e->arg, var))
                return ec_unary(EC_SINH, ec_varc(var));
            break;
        case EC_TANH:
            if (expr_var_matches(e->arg, var))
                return ec_unary(EC_LN, ec_unary(EC_COSH, ec_varc(var)));
            break;

        case EC_MUL: {
            Expr *r = integ_exp_trig(e, var);
            if (r) return r;
            r = integ_tabular(e, var);
            if (r) return r;
            r = integ_by_parts(e, var);
            if (r) return r;
            break;
        }

        case EC_DIV: {
            if (e->left && e->right &&
                e->left->type == EC_NUM && fabs(e->left->num_val - 1.0) < 1e-14 &&
                e->right->type == EC_VAR && e->right->var_name == var) {
                return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
            }
            Expr *r = try_rational_antideriv(e, var);
            if (r) return ec_simplify(r);
            return NULL;
        }

        case EC_COTH:
            if (expr_var_matches(e->arg, var)) {
                return ec_unary(EC_LN, ec_unary(EC_SINH, ec_varc(var)));
            }
            break;
        case EC_SECH:
            if (expr_var_matches(e->arg, var)) {
                Expr *half = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *tanh_half = ec_unary(EC_TANH, half);
                Expr *result = ec_binary(EC_MUL, ec_num(2), ec_unary(EC_ATAN, tanh_half));
                ec_free_expr(half);
                return result;
            }
            break;
        case EC_CSCH:
            if (expr_var_matches(e->arg, var)) {
                Expr *half = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *tanh_half = ec_unary(EC_TANH, half);
                Expr *result = ec_unary(EC_LN, ec_unary(EC_ABS, tanh_half));
                ec_free_expr(half);
                return result;
            }
            break;

        case EC_ACOTH:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_acoth = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ACOTH, ec_varc(var)));
                Expr *ln1 = ec_unary(EC_LN, ec_unary(EC_ABS, ec_binary(EC_SUB, ec_varc(var), ec_num(1))));
                Expr *ln2 = ec_unary(EC_LN, ec_unary(EC_ABS, ec_binary(EC_ADD, ec_varc(var), ec_num(1))));
                Expr *term = ec_binary(EC_MUL, ec_num(0.5), ec_binary(EC_SUB, ln1, ln2));
                return ec_binary(EC_ADD, x_acoth, term);
            }
            break;
        case EC_ASECH:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_asech = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ASECH, ec_varc(var)));
                Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_SUB, ec_num(1), ec_varc(var)));
                Expr *result = ec_binary(EC_ADD, x_asech, ec_binary(EC_MUL, ec_num(2), sqrt_term));
                return result;
            }
            break;
        case EC_ACSCH:
            if (expr_is_simple_var(e->arg, var)) {
                Expr *x_acsch = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ACSCH, ec_varc(var)));
                Expr *sq = ec_binary(EC_POW, ec_varc(var), ec_num(2));
                Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_ADD, sq, ec_num(1)));
                Expr *ln_term = ec_unary(EC_LN, ec_binary(EC_ADD, ec_varc(var), sqrt_term));
                ec_free_expr(sq); ec_free_expr(sqrt_term);
                return ec_binary(EC_ADD, x_acsch, ln_term);
            }
            break;

        default:
            break;
    }
    return ec_intg(ec_copy(e), var);
}

/*============================================================
 * Public API
 *============================================================*/
Expr* ec_integrate(Expr *e, char var) {
    integ_tabular_hit_limit = 0;
    Expr *r = integ_rec(e, var);
    if (r) {
        if (r->type == EC_INT) {
            ec_free_expr(r);
            r = NULL;
        } else {
            return ec_simplify(r);
        }
    }

    if (e) {
        char vbuf[2] = {var, 0};
        Expr *series = ec_series_integral(ec_copy(e), vbuf, 10);
        if (series && series->type != EC_INT) return ec_simplify(series);
        ec_free_expr(series);
    }

    return NULL;
}

Expr* ec_definite_integral(Expr *e, char var, Expr *a, Expr *b) {
    Expr *F = integ_rec(e, var);
    if (!F) return ec_defintg(ec_copy(e), var, a, b);
    ECValue va = ec_eval(a, NULL);
    ECValue vb = ec_eval(b, NULL);
    double av = ec_value_as_real(&va);
    double bv = ec_value_as_real(&vb);
    ec_value_free(&va);
    ec_value_free(&vb);
    Expr *Fa = ec_substitute_val(F, &var, av);
    Expr *Fb = ec_substitute_val(F, &var, bv);
    Expr *result = ec_binary(EC_SUB, Fa, Fb);
    ec_free_expr(F);
    Expr *simplified = ec_simplify(result);
    ec_free_expr(result);
    return simplified;
}

/*============================================================
 * Adaptive Simpson numerical integration
 *============================================================*/
static double eval_point(const Expr *e, char var, double x) {
    ECEnv env;
    env.vars = NULL; env.n = 0;
    char buf[2] = {var, 0};
    ec_env_bind(&env, buf, x);
    ECValue v = ec_eval(e, &env);
    double val = ec_value_as_real(&v);
    ec_value_free(&v);
    ec_env_free(&env);
    return val;
}

static double simpson_rec(const Expr *e, char var,
                           double a, double b,
                           double fa, double fb, double fm,
                           double whole, double tol, int depth) {
    double m = (a + b) * 0.5;
    double lm = (a + m) * 0.5, rm = (m + b) * 0.5;
    double flam = eval_point(e, var, lm);
    (void)m;
    double frm  = eval_point(e, var, rm);
    double left  = (m - a)   * (fa + 4.0*flam + fm)  / 6.0;
    double right = (b - m)   * (fm + 4.0*frm + fb)  / 6.0;
    double est = left + right;
    double err = (est - whole) / 15.0;
    if (depth <= 0 || fabs(err) < tol)
        return est + err;
    double l = simpson_rec(e, var, a, m, fa, fm, flam, left,  tol*0.5, depth-1);
    double r = simpson_rec(e, var, m, b, fm, fb, frm, right, tol*0.5, depth-1);
    return l + r;
}

static double adaptive_simpson(const Expr *e, char var,
                               double a, double b,
                               double tol, int max_depth) {
    double fa = eval_point(e, var, a);
    double fb = eval_point(e, var, b);
    double fm = eval_point(e, var, (a + b) * 0.5);
    double whole = (b - a) * (fa + 4.0*fm + fb) / 6.0;
    return simpson_rec(e, var, a, b, fa, fb, fm, whole, tol, max_depth);
}

double ec_numerical_integral(Expr *e, char var, double a, double b,
                              const char *method) {
    (void)method;
    if (!e) return 0.0;
    return adaptive_simpson(e, var, a, b, 1e-8, 20);
}

double ec_numerical_integral_ex(Expr *e, char var, double a, double b,
                                  ECIntMethod method, double tol) {
    (void)method;
    if (!e) return 0.0;
    if (tol <= 0) tol = 1e-8;
    return adaptive_simpson(e, var, a, b, tol, 25);
}

/*============================================================
 * Integration with derivation steps
 *============================================================*/
Derivation* ec_integrate_with_steps(Expr *e, char var) {
    Derivation *d = ec_derivation_new();
    char buf[512];
    char *orig = ec_to_latex(e);
    snprintf(buf, sizeof(buf), "\\int %s \\, d%c", orig, var);
    ec_derivation_add(d, buf, "Initial Integral");
    ec_free(orig);

    Expr *r = integ_rec(e, var);
    if (r) {
        Expr *sr = ec_simplify(r);
        char *res = ec_to_latex(sr);
        snprintf(buf, sizeof(buf), "%s + C", res);
        ec_derivation_add(d, buf, "Indefinite Integral");
        ec_free(res);
        ec_free_expr(sr);
        ec_free_expr(r);
    } else {
        char *unres = ec_to_latex(e);
        snprintf(buf, sizeof(buf), "\\int %s \\, d%c  \\text{(no closed form)}", unres, var);
        ec_derivation_add(d, buf, "No Closed Form");
        ec_free(unres);
    }
    return d;
}
