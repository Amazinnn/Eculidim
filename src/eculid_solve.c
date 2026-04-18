#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Helpers
 *============================================================*/
/* Evaluate expression at a point */
static double eval_at(const Expr *e, const char *var, double val) {
    if (!e) return 0.0;
    ECEnv env = ec_env_new();
    ec_env_bind(&env, var, val);
    ECValue v = ec_eval(e, &env);
    double r = ec_value_as_real(&v);
    ec_value_free(&v);
    ec_env_free(&env);
    return r;
}

/* Check if an expression is effectively zero */
static int is_zero(double v) { return fabs(v) < 1e-14; }

/*============================================================
 * ec_normalize_equation — move all terms to LHS, set = 0
 *   Input: x^2 - 4 = 0  or  x^2 - 4  (already implicit = 0)
 *   Output: left side of normalized equation
 *============================================================*/
Expr* ec_normalize_equation(Expr *eq) {
    if (!eq) return NULL;
    /* If top-level is EC_EQ: move everything to left: lhs - rhs = 0 */
    if (eq->type == EC_EQ) {
        Expr *lhs = eq->left;
        Expr *rhs = eq->right;
        Expr *diff = ec_binary(EC_SUB, ec_copy(lhs), ec_copy(rhs));
        return ec_simplify(diff);
    }
    /* Already a bare expression (implicit = 0) */
    return ec_copy(eq);
}

/*============================================================
 * Extract polynomial coefficients for linear/quadratic solving
 * Returns degree of polynomial in var, fills in coeffs array
 *============================================================*/
static int extract_coeffs(const Expr *e, char var,
                          double *coeffs, int max_deg) {
    if (!e) return 0;
    switch (e->type) {
        case EC_NUM:
            coeffs[0] += e->num_val;
            return 0;
        case EC_VAR:
            if (e->var_name == var) { coeffs[1] += 1.0; return 1; }
            return 0;
        case EC_ADD:
        case EC_SUB: {
            int d1 = extract_coeffs(e->left, var, coeffs, max_deg);
            int d2;
            if (e->type == EC_SUB) {
                /* Temporarily negate right coefficients */
                double tmp[20] = {0};
                d2 = extract_coeffs(e->right, var, tmp, max_deg);
                for (int i = 0; i <= d2 && i < max_deg; i++)
                    coeffs[i] -= tmp[i];
            } else {
                d2 = extract_coeffs(e->right, var, coeffs, max_deg);
            }
            return d1 > d2 ? d1 : d2;
        }
        case EC_NEG: {
            double tmp[20] = {0};
            int d = extract_coeffs(e->arg, var, tmp, max_deg);
            for (int i = 0; i <= d && i < max_deg; i++)
                coeffs[i] -= tmp[i];
            return d;
        }
        case EC_MUL: {
            /* c * x^n pattern */
            double c = 1.0;
            const Expr *base = NULL;
            int pow_n = 0;
            /* Check for constant factor */
            if (e->left && e->left->type == EC_NUM)
                c = e->left->num_val;
            else if (e->right && e->right->type == EC_NUM)
                c = e->right->num_val;
            /* Find the variable base */
            if (e->left && e->left->type == EC_VAR && e->left->var_name == var)
                base = e->left;
            else if (e->left && e->left->type == EC_POW
                     && e->left->left && e->left->left->type == EC_VAR
                     && e->left->left->var_name == var
                     && e->left->right && e->left->right->type == EC_NUM) {
                base = e->left->left;
                pow_n = (int)e->left->right->num_val;
            } else if (e->right && e->right->type == EC_VAR && e->right->var_name == var)
                base = e->right;
            else if (e->right && e->right->type == EC_POW
                     && e->right->left && e->right->left->type == EC_VAR
                     && e->right->left->var_name == var
                     && e->right->right && e->right->right->type == EC_NUM) {
                base = e->right->left;
                pow_n = (int)e->right->right->num_val;
            }
            if (base) {
                int deg = pow_n > 0 ? pow_n : 1;
                if (deg < max_deg) {
                    coeffs[deg] += c;
                    return deg;
                }
            }
            return 0;
        }
        case EC_POW: {
            if (e->left && e->left->type == EC_VAR && e->left->var_name == var
                && e->right && e->right->type == EC_NUM) {
                int deg = (int)e->right->num_val;
                if (deg < max_deg) {
                    coeffs[deg] += 1.0;
                    return deg;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

/*============================================================
 * ec_solve_linear — solve a*x + b = 0
 *============================================================*/
Expr* ec_solve_linear(Expr *eq, char var) {
    Expr *norm = ec_normalize_equation(eq);
    double coeffs[20] = {0};
    int deg = extract_coeffs(norm, var, coeffs, 20);
    ec_free_expr(norm);
    if (deg != 1) return NULL;
    if (is_zero(coeffs[1])) return NULL; /* no solution */
    double x = -coeffs[0] / coeffs[1];
    return ec_num(x);
}

/*============================================================
 * ec_solve_quadratic — solve ax^2 + bx + c = 0 (returns symbolic root)
 *============================================================*/
Expr* ec_solve_quadratic(double a, double b, double c) {
    double d = b * b - 4.0 * a * c;
    if (d < 0) return NULL; /* no real roots */
    if (d == 0)
        return ec_num(-b / (2.0 * a));
    /* Two distinct real roots */
    Expr *r1 = ec_num((-b + sqrt(d)) / (2.0 * a));
    Expr *r2 = ec_num((-b - sqrt(d)) / (2.0 * a));
    return ec_binary(EC_ADD,
        ec_binary(EC_SUB, r1, ec_num(0)),
        ec_binary(EC_SUB, r2, ec_num(0)));
}

/*============================================================
 * Newton-Raphson
 *============================================================*/
static double newton_raphson(Expr *f_expr, const char *var,
                              double x0, double tol, int max_iter, int *n_iter) {
    Expr *df = ec_diff(f_expr, var[0]);
    *n_iter = 0;
    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double fx = eval_at(f_expr, var, x);
        if (fabs(fx) < tol) { *n_iter = i; ec_free_expr(df); return x; }
        double dfx = eval_at(df, var, x);
        if (is_zero(dfx)) break;
        x = x - fx / dfx;
        (*n_iter)++;
    }
    ec_free_expr(df);
    return x;
}

double* ec_solve_numeric(Expr *eq, const char *var,
                          double x0, double x1, int *n_iter) {
    Expr *norm = ec_normalize_equation(eq);
    double fx0 = eval_at(norm, var, x0);
    double fx1 = eval_at(norm, var, x1);
    /* Try bisection first */
    if (fx0 * fx1 < 0) {
        double a = x0, b = x1;
        for (int i = 0; i < 100; i++) {
            double m = (a + b) * 0.5;
            double fm = eval_at(norm, var, m);
            if (fabs(fm) < 1e-12) {
                double *roots = ec_malloc(sizeof(double));
                roots[0] = m;
                *n_iter = i;
                ec_free_expr(norm);
                return roots;
            }
            if (fm * eval_at(norm, var, a) < 0) b = m;
            else a = m;
        }
        double *roots = ec_malloc(sizeof(double));
        roots[0] = (a + b) * 0.5;
        *n_iter = 100;
        ec_free_expr(norm);
        return roots;
    }
    ec_free_expr(norm);
    /* Try Newton from x0 */
    Expr *norm2 = ec_normalize_equation(eq);
    int nit = 0;
    double x = newton_raphson(norm2, var, x0, 1e-12, 200, &nit);
    ec_free_expr(norm2);
    double *roots = ec_malloc(sizeof(double));
    roots[0] = x;
    *n_iter = nit;
    return roots;
}

double ec_solve_numeric_ex(Expr *eq, const char *var,
                             double x0, double x1, ECSolveMethod method,
                             double tol, int max_iter, int *n_iter) {
    if (!eq) return 0.0;
    Expr *norm = ec_normalize_equation(eq);
    double result = 0.0;
    switch (method) {
        case EC_NEWTON: {
            int nit = 0;
            result = newton_raphson(norm, var, x0, tol, max_iter, &nit);
            *n_iter = nit;
            break;
        }
        case EC_BISECTION: {
            double a = x0, b = x1;
            double fa = eval_at(norm, var, a);
            double fb = eval_at(norm, var, b);
            if (fa * fb > 0) { ec_free_expr(norm); *n_iter = 0; return (x0+x1)*0.5; }
            for (int i = 0; i < max_iter; i++) {
                double m = (a + b) * 0.5;
                double fm = eval_at(norm, var, m);
                if (fabs(fm) < tol || (b-a)*0.5 < tol) { result = m; *n_iter = i; break; }
                if (fm * fa < 0) { b = m; fb = fm; }
                else { a = m; fa = fm; }
                result = m;
            }
            *n_iter = max_iter;
            break;
        }
        case EC_SECANT: {
            double xn = x0, xn1 = x1;
            for (int i = 0; i < max_iter; i++) {
                double fxn  = eval_at(norm, var, xn);
                double fxn1 = eval_at(norm, var, xn1);
                if (fabs(fxn1 - fxn) < 1e-15) break;
                double xn2 = xn1 - fxn1 * (xn1 - xn) / (fxn1 - fxn);
                xn = xn1; xn1 = xn2;
                if (fabs(fxn1) < tol) { *n_iter = i; ec_free_expr(norm); return xn1; }
                result = xn1;
            }
            *n_iter = max_iter;
            break;
        }
        case EC_BRENTQ: {
            double a = x0, b = x1;
            double fa = eval_at(norm, var, a);
            double fb = eval_at(norm, var, b);
            if (fa * fb > 0) { ec_free_expr(norm); *n_iter = 0; return (x0+x1)*0.5; }
            double c = a, d = b, e = b - a;
            double fc = fa;
            for (int i = 0; i < max_iter; i++) {
                if (fabs(fc) < fabs(fb)) {
                    a = b; b = c; c = a;
                    fa = fb; fb = fc; fc = fa;
                }
                double tol1 = 2e-14 * fabs(b) + tol * 0.5;
                double m = (a + b) * 0.5;
                if (fabs(m-b) <= tol1 || is_zero(fb)) { result = b; *n_iter = i; ec_free_expr(norm); return result; }
                if (fabs(e) < tol1 || fabs(fa) <= fabs(fb)) {
                    d = m; e = m - b;
                } else {
                    double s = b;
                    if (a != c && fa != fc) {
                        double q = fa/(fa-fc), p = (fa-q*fa+(1.0-q)*fb)/(fa-fb);
                        if (4.0*p*q < (b-a)*q*(q-1.0)) {
                            s = b + p / q * (b - a);
                            if (fabs(s-b) < fabs((b-a)*0.5)) {
                                d = s; e = s - b;
                            }
                        }
                    }
                    if (fabs(e) < fabs((b-a)*0.5) || fabs(e) >= fabs((b-a)*0.5)) {
                        d = m; e = m - b;
                    }
                }
                a = b; fa = fb;
                if (fabs(d) > tol1) { b = b + d; }
                else { b = b + (m-b >= 0 ? tol1 : -tol1); }
                fb = eval_at(norm, var, b);
                result = b;
            }
            *n_iter = max_iter;
            break;
        }
        default:
            *n_iter = 0;
            break;
    }
    ec_free_expr(norm);
    return result;
}

/*============================================================
 * Main dispatcher: ec_solve
 *============================================================*/
EculidRoots ec_solve(Expr *eq, const char *var) {
    EculidRoots r = {NULL, 0, NULL};
    if (!eq || !var || !var[0]) return r;

    Expr *norm = ec_normalize_equation(eq);
    char v = var[0];

    /* Extract polynomial coefficients */
    double coeffs[20] = {0};
    int deg = extract_coeffs(norm, v, coeffs, 20);
    (void)deg;

    /* Check degree and solve */
    if (deg == 0) {
        /* Constant equation: c = 0 */
        if (!is_zero(coeffs[0])) {
            /* No solution */
        }
        ec_free_expr(norm);
        return r;
    }

    if (deg == 1) {
        /* Linear: a*x + b = 0 */
        Expr *sol = ec_solve_linear(eq, v);
        if (sol) {
            r.roots = ec_malloc(sizeof(Expr*));
            r.roots[0] = sol;
            r.count = 1;
            r.method = ec_strdup("Linear Formula");
        }
        ec_free_expr(norm);
        return r;
    }

    if (deg == 2) {
        /* Quadratic: ax^2 + bx + c = 0 */
        double a = coeffs[2], b = coeffs[1], c = coeffs[0];
        double d = b*b - 4.0*a*c;
        if (d < 0) {
            /* No real roots */
        } else if (d == 0) {
            r.roots = ec_malloc(sizeof(Expr*));
            r.roots[0] = ec_num(-b / (2.0 * a));
            r.count = 1;
            r.method = ec_strdup("Quadratic Formula");
        } else {
            r.roots = ec_malloc(2 * sizeof(Expr*));
            r.roots[0] = ec_num((-b + sqrt(d)) / (2.0 * a));
            r.roots[1] = ec_num((-b - sqrt(d)) / (2.0 * a));
            r.count = 2;
            r.method = ec_strdup("Quadratic Formula");
        }
        ec_free_expr(norm);
        return r;
    }

    ec_free_expr(norm);

    /* For higher-degree, fall back to numeric */
    /* Use derivative to find good initial guesses */
    Expr *f_expr = ec_normalize_equation(eq);
    Expr *df_expr = ec_diff(f_expr, v);

    /* Try to find sign changes in [-10, 10] */
    double step = 0.5;
    int capacity = 4;
    double *roots_tmp = ec_malloc(capacity * sizeof(double));
    int n_roots = 0;

    for (double x = -10.0; x <= 10.0; x += step) {
        double fx  = eval_at(f_expr, var, x);
        double fx1 = eval_at(f_expr, var, x + step);
        if (fx * fx1 <= 0) {
            double root = ec_solve_numeric_ex(eq, var, x, x + step, EC_BRENTQ, 1e-12, 200, NULL);
            /* Avoid duplicates */
            int dup = 0;
            for (int j = 0; j < n_roots; j++)
                if (fabs(roots_tmp[j] - root) < 1e-6) { dup = 1; break; }
            if (!dup) {
                if (n_roots >= capacity) {
                    capacity *= 2;
                    roots_tmp = ec_realloc(roots_tmp, capacity * sizeof(double));
                }
                roots_tmp[n_roots++] = root;
            }
        }
    }
    ec_free_expr(f_expr);
    ec_free_expr(df_expr);

    if (n_roots > 0) {
        r.roots = ec_malloc(n_roots * sizeof(Expr*));
        r.count = n_roots;
        for (int i = 0; i < n_roots; i++)
            r.roots[i] = ec_num(roots_tmp[i]);
        r.method = ec_strdup("Brentq (numeric)");
    }
    free(roots_tmp);
    return r;
}

/*============================================================
 * Polynomial factoring stub
 *============================================================*/
EculidRoots ec_solve_poly(Expr *e, const char *var) {
    return ec_solve(e, var);
}

int ec_factor_poly(Expr *e, const char *var,
                    Expr **factors, int max_factors) {
    (void)e; (void)var; (void)factors; (void)max_factors;
    return 0;
}

/*============================================================
 * Inequality solving stub
 *============================================================*/
ECInterval* ec_solve_inequality(Expr *ineq, const char *var, int *count) {
    (void)ineq; (void)var; *count = 0; return NULL;
}

/*============================================================
 * Memory management
 *============================================================*/
void ec_roots_free(EculidRoots *r) {
    if (!r) return;
    for (int i = 0; i < r->count; i++)
        ec_free_expr(r->roots[i]);
    if (r->roots) ec_free(r->roots);
    if (r->method) ec_free(r->method);
    r->roots = NULL; r->count = 0; r->method = NULL;
}

void ec_interval_free(ECInterval *iv, int n) {
    (void)iv; (void)n;
}
