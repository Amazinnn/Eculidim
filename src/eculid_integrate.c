#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Internal helpers
 *============================================================*/
static int expr_is_simple_var(const Expr *e, char var) {
    return e && e->type == EC_VAR && e->var_name == var && e->var_str == NULL;
}

static int node_has_integral(const Expr *e, char var);

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
 * Internal recursive integrator
 *============================================================*/
static Expr* integ_rec(const Expr *e, char var) {
    if (!e) return NULL;
    switch (e->type) {
        /*---- Constants ----*/
        case EC_NUM:
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        case EC_VAR:
            if (e->var_name == var)
                return ec_binary(EC_DIV, ec_binary(EC_POW, ec_varc(var), ec_num(2)), ec_num(2));
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        case EC_PI: case EC_E: case EC_I:
            return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));

        /*---- Arithmetic ----*/
        case EC_ADD:
            return ec_binary(EC_ADD, integ_rec(e->left, var), integ_rec(e->right, var));

        case EC_SUB:
            return ec_binary(EC_SUB, integ_rec(e->left, var), integ_rec(e->right, var));

        case EC_NEG:
            return ec_unary(EC_NEG, integ_rec(e->arg, var));

        case EC_POW: {
            /* Power rule: x^n, n != -1 */
            if (e->left && e->left->type == EC_VAR && e->left->var_name == var) {
                if (e->right && e->right->type == EC_NUM) {
                    double n = e->right->num_val;
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
            /* Composite base: (ax+b)^n */
            if (e->right && e->right->type == EC_NUM) {
                double n = e->right->num_val;
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

        /*---- Trigonometric ----*/
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

        /*---- Exponential ----*/
        case EC_EXP:
            if (expr_is_simple_var(e->arg, var))
                return ec_copy(e);
            { Expr *du = ec_diff(e->arg, var);
              return ec_binary(EC_DIV, ec_copy(e), ec_copy(du)); }
            break;

        /*---- Logarithmic ----*/
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

        /*---- Root functions ----*/
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

        /*---- Inverse trig ----*/
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

        /*---- Hyperbolic ----*/
        case EC_SINH:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_COSH, ec_varc(var));
            break;
        case EC_COSH:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_SINH, ec_varc(var));
            break;
        case EC_TANH:
            if (expr_is_simple_var(e->arg, var))
                return ec_unary(EC_LN, ec_unary(EC_COSH, ec_varc(var)));
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
    Expr *r = integ_rec(e, var);
    return r ? ec_simplify(r) : NULL;
}

Expr* ec_definite_integral(Expr *e, char var, Expr *a, Expr *b) {
    Expr *F = integ_rec(e, var);
    if (!F) return ec_defintg(ec_copy(e), var, a, b);
    /* Evaluate F at upper and lower bounds */
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
