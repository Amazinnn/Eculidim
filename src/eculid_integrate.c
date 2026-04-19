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

static int expr_var_matches(const Expr *e, char var) {
    return e && e->type == EC_VAR && e->var_name == var;
}

static int node_has_integral(const Expr *e, char var);
static Expr* integ_rec(const Expr *e, char var);

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
    /* ∫u·dv = u·v - ∫v·du
     * Strategy: u = polynomial part, dv = the rest.
     * Returns NULL if no pattern matches.
     */
    if (e->type != EC_MUL) return NULL;

    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    /* Normalize: ensure A is the polynomial */
    if (B->type == EC_POW &&
        B->right && B->right->type == EC_NUM && B->right->num_val >= 0 &&
        B->left && B->left->type == EC_VAR && B->left->var_name == var) {
        /* B is x^n, A is something else: swap */
        A = e->right; B = e->left;
    }

    /* Check if A is polynomial in var: x, x^2, x^3, ... */
    int poly_deg = -1;
    if (A->type == EC_VAR && A->var_name == var && !A->var_str)
        poly_deg = 1;
    else if (A->type == EC_POW &&
             A->left && A->left->type == EC_VAR && A->left->var_name == var &&
             A->right && A->right->type == EC_NUM && A->right->num_val >= 0 &&
             !A->left->var_str)
        poly_deg = (int)(A->right->num_val + 0.5);
    else if (A->type == EC_NUM)
        poly_deg = 0; /* constant factor: ∫c·f(x)dx = c·∫f(x)dx */

    if (poly_deg < 0) return NULL; /* no polynomial factor */

    /* u = polynomial, dv = B dx */
    Expr *u, *du, *v;
    if (poly_deg == 0) {
        /* ∫c·B = c·∫B, already handled by linearity */
        return NULL;
    } else {
        /* u = A, dv = B dx → du = A' dx, v = ∫B dx */
        u = ec_copy(A);
        du = ec_diff(ec_copy(A), var);
        Expr *IB = integ_rec(B, var);
        if (!IB) { ec_free_expr(u); ec_free_expr(du); return NULL; }
        v = IB;
    }

    /* ∫A·B = u·v - ∫v·du */
    Expr *uv = ec_binary(EC_MUL, u, v);
    Expr *ivdu = integ_rec(ec_binary(EC_MUL, v, du), var);
    Expr *result;
    if (ivdu) {
        result = ec_binary(EC_SUB, uv, ivdu);
    } else {
        /* ∫v·du also unresolved — just return u·v as partial */
        result = uv;
        ec_free_expr(ivdu);
    }
    ec_free_expr(v);
    ec_free_expr(du);
    return ec_simplify(result);
}

/*============================================================
 * Rational function antiderivatives
 *============================================================*/
static Expr* try_rational_antideriv(const Expr *e, char var) {
    /* Detect 1/(x^2+1) → atan(x) */
    if (e->type == EC_DIV) {
        const Expr *num = e->left;
        const Expr *den = e->right;
        /* Pattern: 1/(x^2+1) */
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
        /* Pattern: 1/x */
        if (num && den &&
            num->type == EC_NUM && num->num_val == 1.0 &&
            den->type == EC_VAR && den->var_name == var) {
            return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
        }
    }
    return NULL;
}

/*============================================================
 * Internal recursive integrator
 *============================================================*/
static Expr* integ_rec(const Expr *e, char var) {
    if (!e) return NULL;

    /* Unwrap EC_INT wrapper (from \int parsed input) */
    if (e->type == EC_INT)
        return integ_rec(e->arg, e->deriv_var ? e->deriv_var : var);

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
            /* Extract numeric exponent, handling EC_NEG(EC_NUM) too */
            double n = 0.0;
            int have_num = 0;
            if (e->right && e->right->type == EC_NUM) {
                n = e->right->num_val; have_num = 1;
            } else if (e->right && e->right->type == EC_NEG &&
                       e->right->arg && e->right->arg->type == EC_NUM) {
                n = -e->right->arg->num_val; have_num = 1;
            }
            /* Power rule: x^n, n != -1 */
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
            /* Composite base: (ax+b)^n */
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

        /*---- Products: integration by parts ----*/
        case EC_MUL: {
            Expr *r = integ_by_parts(e, var);
            if (r) return r;
            break;
        }

        /*---- Rational functions: arctan, 1/x ----*/
        case EC_DIV: {
            /* Pattern: 1/x → ln|x| (var_str may be set when parsed via \int mode) */
            if (e->left && e->right &&
                e->left->type == EC_NUM && fabs(e->left->num_val - 1.0) < 1e-14 &&
                e->right->type == EC_VAR && e->right->var_name == var) {
                return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
            }
            /* Pattern: 1/(x^2+1) → atan(x) */
            Expr *r = try_rational_antideriv(e, var);
            if (r) return ec_simplify(r);
            /* Don't fall through to broken formula: give up */
            return NULL;
        }

        /*---- More hyperbolic ----*/
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

        /*---- Inverse hyperbolic ----*/
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
