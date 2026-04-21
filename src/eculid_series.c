#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Helpers
 *============================================================*/
static double eval_point(const Expr *e, char var, double val) {
    if (!e) return 0.0;
    ECEnv env = ec_env_new();
    char buf[2] = {var, 0};
    ec_env_bind(&env, buf, val);
    ECValue v = ec_eval(e, &env);
    double r = ec_value_as_real(&v);
    ec_value_free(&v);
    ec_env_free(&env);
    return r;
}

/*============================================================
 * ec_series — generic dispatcher
 *============================================================*/
Expr* ec_series(Expr *e, const char *var, const Expr *point,
                 int order, ECSeriesType type) {
    if (!e) return NULL;
    switch (type) {
        case EC_SERIES_MACLAURIN:
            return ec_maclaurin(e, var, order);
        case EC_SERIES_TAYLOR:
            return ec_taylor(e, var, point, order);
        case EC_SERIES_LAGRANGE:
        case EC_SERIES_PUISEUX:
            /* Not yet implemented: fall back to Taylor */
            return ec_taylor(e, var, point, order);
        default:
            return ec_taylor(e, var, point, order);
    }
}

/*============================================================
 * ec_taylor — Taylor expansion around point a
 *   T(x) = sum_{k=0}^{n} f^{(k)}(a)/k! * (x-a)^k
 *============================================================*/
Expr* ec_taylor(Expr *e, const char *var, const Expr *a, int n) {
    if (!e || !var) return NULL;
    char v = var[0];
    if (n < 0) n = 0;

    /* Evaluate the expansion point: default to 0 (Maclaurin) */
    double a_val = 0.0;
    int have_a = 1; /* always have a numeric a for evaluation */

    if (a) {
        if (a->type == EC_NUM) {
            a_val = a->num_val;
        } else {
            ECValue av = ec_eval(a, NULL);
            if (av.type == EC_VAL_REAL) { a_val = av.real; }
            else { have_a = 0; }
            ec_value_free(&av);
        }
    }

    /* Build sum_{k=0}^{n} f^{(k)}(a)/k! * (var-a)^k */
    Expr *result = NULL;

    for (int k = 0; k <= n; k++) {
        /* Compute f^{(k)}(e) */
        Expr *deriv_k = ec_diff_n(e, v, k);
        if (!deriv_k) { deriv_k = ec_num(0); }

        /* Evaluate f^{(k)}(a) numerically via environment */
        double fk_val;
        if (have_a) {
            fk_val = eval_point(deriv_k, var[0], a_val);
        } else {
            fk_val = 0.0;
        }

        /* k! */
        double fact_k = 1.0;
        for (int j = 2; j <= k; j++) fact_k *= (double)j;

        /* (var - a)^k */
        Expr *var_minus_a;
        if (have_a) {
            var_minus_a = ec_binary(EC_SUB, ec_varc(v), ec_num(a_val));
        } else if (a) {
            var_minus_a = ec_binary(EC_SUB, ec_varc(v), ec_copy(a));
        } else {
            var_minus_a = ec_varc(v);
        }

        Expr *pow_term;
        if (k == 0) {
            pow_term = ec_num(1);
            ec_free_expr(var_minus_a);
        } else {
            pow_term = ec_binary(EC_POW, var_minus_a, ec_num((double)k));
        }

        Expr *term = ec_binary(EC_MUL, ec_num(fk_val / fact_k), pow_term);
        ec_free_expr(deriv_k);

        if (!result) result = term;
        else { Expr *new_r = ec_binary(EC_ADD, result, term); result = new_r; }
    }

    if (!result) result = ec_num(0);
    return ec_simplify(result);
}

/*============================================================
 * ec_maclaurin — Taylor at a = 0
 *============================================================*/
Expr* ec_maclaurin(Expr *e, const char *var, int n) {
    return ec_taylor(e, var, NULL, n);
}

/*============================================================
 * Series integral helper (termwise over polynomial-like expansion)
 *============================================================*/
static Expr* integrate_series_termwise(const Expr *e, char var) {
    if (!e) return NULL;

    if (e->type == EC_ADD)
        return ec_binary(EC_ADD,
            integrate_series_termwise(e->left, var),
            integrate_series_termwise(e->right, var));

    if (e->type == EC_SUB)
        return ec_binary(EC_SUB,
            integrate_series_termwise(e->left, var),
            integrate_series_termwise(e->right, var));

    if (e->type == EC_NEG)
        return ec_unary(EC_NEG, integrate_series_termwise(e->arg, var));

    if (e->type == EC_NUM) {
        return ec_binary(EC_MUL, ec_copy(e), ec_varc(var));
    }

    if (e->type == EC_VAR && e->var_name == var) {
        return ec_binary(EC_DIV,
            ec_binary(EC_POW, ec_varc(var), ec_num(2)),
            ec_num(2));
    }

    if (e->type == EC_POW &&
        e->left && e->left->type == EC_VAR && e->left->var_name == var &&
        e->right && e->right->type == EC_NUM) {
        double n = e->right->num_val;
        if (fabs(n + 1.0) < 1e-12)
            return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
        return ec_binary(EC_MUL,
            ec_num(1.0 / (n + 1.0)),
            ec_binary(EC_POW, ec_varc(var), ec_num(n + 1.0)));
    }

    if (e->type == EC_MUL) {
        /* c*x^n */
        const Expr *num = NULL;
        const Expr *pow = NULL;
        if (e->left && e->left->type == EC_NUM) { num = e->left; pow = e->right; }
        else if (e->right && e->right->type == EC_NUM) { num = e->right; pow = e->left; }

        if (num && pow) {
            if (pow->type == EC_VAR && pow->var_name == var) {
                return ec_binary(EC_MUL, ec_num(num->num_val / 2.0),
                    ec_binary(EC_POW, ec_varc(var), ec_num(2)));
            }
            if (pow->type == EC_POW &&
                pow->left && pow->left->type == EC_VAR && pow->left->var_name == var &&
                pow->right && pow->right->type == EC_NUM) {
                double n = pow->right->num_val;
                if (fabs(n + 1.0) < 1e-12)
                    return ec_binary(EC_MUL, ec_num(num->num_val),
                        ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var))));
                return ec_binary(EC_MUL,
                    ec_num(num->num_val / (n + 1.0)),
                    ec_binary(EC_POW, ec_varc(var), ec_num(n + 1.0)));
            }
        }
    }

    /* Unknown term: keep as formal integral wrapper */
    return ec_intg(ec_copy(e), var);
}

/*============================================================
 * Taylor-series termwise integration fallback
 *============================================================*/
Expr* ec_series_integral(Expr *e, const char *var, int order) {
    if (!e || !var || order <= 0) return NULL;
    Expr *series = ec_maclaurin(e, var, order);
    if (!series) return NULL;
    Expr *integrated = integrate_series_termwise(series, var[0]);
    ec_free_expr(series);
    if (!integrated) return NULL;
    return ec_simplify(integrated);
}

/*============================================================
 * ec_series_revert — inverse series expansion
 *   Given y = sum c_k * (x-a)^k, find x as a series in y
 *   Not implemented: return NULL
 *============================================================*/
Expr* ec_series_revert(Expr *s, const char *var, int n) {
    (void)s; (void)var; (void)n;
    return NULL; /* TODO: Newton's method based series reversion */
}

/*============================================================
 * ec_series_with_steps — Taylor expansion with derivation steps
 *============================================================*/
Derivation* ec_series_with_steps(Expr *e, const char *var,
                                  const Expr *a, int n) {
    Derivation *d = ec_derivation_new();
    char buf[512];
    char *orig = ec_to_latex(e);
    char *pt_str = a ? ec_to_latex(a) : ec_strdup("0");
    snprintf(buf, sizeof(buf), "T_%d[%s](%s) = \\sum_{k=0}^{%d} \\frac{f^{(k)}(%s)}{k!}(%s-%s)^k",
             n, orig, pt_str, n, pt_str, var, pt_str);
    ec_derivation_add(d, buf, "Taylor Series Definition");
    ec_free(orig); ec_free(pt_str);

    Expr *result = ec_taylor(e, var, a, n);
    if (result) {
        char *res = ec_to_latex(result);
        snprintf(buf, sizeof(buf), "%s + O((%s-a)^{%d})", res, var, n+1);
        ec_derivation_add(d, buf, "Taylor Polynomial");
        ec_free(res);
        ec_free_expr(result);
    } else {
        ec_derivation_add(d, "\\text{Unable to compute series}", "No Rule");
    }
    return d;
}
