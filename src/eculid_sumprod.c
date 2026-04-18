#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Helpers
 *============================================================*/
static int is_constant_expr(const Expr *e) {
    if (!e) return 1;
    if (e->type == EC_VAR) return 0;
    if (e->type == EC_NUM) return 1;
    return is_constant_expr(e->left) && is_constant_expr(e->right)
        && is_constant_expr(e->arg) && is_constant_expr(e->cond);
}

static double eval_with_var(const Expr *e, char var, double val) {
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
 * ec_sum — create a symbolic sum node
 *============================================================*/
Expr* ec_sum(Expr *expr, const char *var, Expr *lo, Expr *hi) {
    return ec_sum_node(expr ? ec_copy(expr) : NULL, var[0], lo, hi);
}

Expr* ec_product(Expr *expr, const char *var, Expr *lo, Expr *hi) {
    return ec_product_node(expr ? ec_copy(expr) : NULL, var[0], lo, hi);
}

/*============================================================
 * ec_sum_eval — numerically evaluate sum_{k=lo}^{hi} expr
 *============================================================*/
double ec_sum_eval(Expr *expr, const char *var, double lo, double hi) {
    if (!expr || !var) return 0.0;
    char v = var[0];
    double result = 0.0;
    int lo_int = (int)lo;
    int hi_int = (int)hi;

    if (lo == lo_int && hi == hi_int && hi_int - lo_int < 100000) {
        for (int k = lo_int; k <= hi_int; k++)
            result += eval_with_var(expr, v, (double)k);
    }
    return result;
}

double ec_product_eval(Expr *expr, const char *var, double lo, double hi) {
    if (!expr || !var) return 1.0;
    char v = var[0];
    double result = 1.0;
    int lo_int = (int)lo;
    int hi_int = (int)hi;
    if (lo == lo_int && hi == hi_int && hi_int - lo_int < 100000) {
        for (int k = lo_int; k <= hi_int; k++) {
            double val = eval_with_var(expr, v, (double)k);
            result *= val;
        }
    }
    return result;
}

/*============================================================
 * ec_sum_closed_form — closed form lookup table
 *============================================================*/
int ec_sum_closed_form(Expr *s, char var, Expr **result) {
    if (!s || !result) return 0;

    const Expr *body = s->arg;
    if (!body) body = s;

    int lo_int = 1, hi_int = 0;
    if (s->left && s->left->type == EC_NUM) lo_int = (int)s->left->num_val;
    if (s->right && s->right->type == EC_NUM) hi_int = (int)s->right->num_val;
    double a = (double)lo_int;
    double b = (double)hi_int;
    double n = b - a + 1.0;

    /* ---- Pattern: constant c ---- */
    if (is_constant_expr(body)) {
        double c = 0.0;
        if (body->type == EC_NUM) c = body->num_val;
        *result = ec_num(c * n);
        return 1;
    }

    /* ---- Pattern: k ---- */
    if (body->type == EC_VAR && body->var_name == var) {
        double sum = n * (a + b) / 2.0;
        *result = ec_num(sum);
        return 1;
    }

    /* ---- Pattern: k^p (p = 1, 2, 3) ---- */
    if (body->type == EC_POW) {
        const Expr *base = body->left;
        const Expr *exp = body->right;
        int is_k = base && base->type == EC_VAR && base->var_name == var;
        if (is_k && exp && exp->type == EC_NUM) {
            double p = exp->num_val;
            if (p == 1.0) {
                *result = ec_num(n * (a + b) / 2.0);
                return 1;
            }
            if (p == 2.0) {
                /* Σ k^2 = n(n+1)(2n+1)/6 */
                double sum = n * (n + 1.0) * (2.0*n + 1.0) / 6.0;
                *result = ec_num(sum);
                return 1;
            }
            if (p == 3.0) {
                /* Σ k^3 = [n(n+1)/2]^2 */
                double sum = n * (n + 1.0) / 2.0;
                sum = sum * sum;
                *result = ec_num(sum);
                return 1;
            }
        }
    }

    /* ---- Pattern: c * k ---- */
    if (body->type == EC_MUL) {
        double c = 1.0;
        const Expr *poly = NULL;
        if (body->left && body->left->type == EC_NUM)
            { c = body->left->num_val; poly = body->right; }
        else if (body->right && body->right->type == EC_NUM)
            { c = body->right->num_val; poly = body->left; }
        if (poly && poly->type == EC_VAR && poly->var_name == var) {
            double sum = c * n * (a + b) / 2.0;
            *result = ec_num(sum);
            return 1;
        }
    }

    /* ---- Pattern: a^k (geometric series) ---- */
    if (body->type == EC_POW) {
        const Expr *base = body->left;
        const Expr *exp = body->right;
        int is_exp_k = exp && exp->type == EC_VAR && exp->var_name == var;
        if (is_exp_k && base && base->type == EC_NUM) {
            double a_base = base->num_val;
            if (fabs(a_base - 1.0) < 1e-14) {
                *result = ec_num(n);
                return 1;
            }
            double a_lo = pow(a_base, a);
            double sum = a_lo * (pow(a_base, n) - 1.0) / (a_base - 1.0);
            *result = ec_num(sum);
            return 1;
        }
    }

    *result = NULL;
    return 0;
}

/*============================================================
 * ec_known_sum — string-based closed form lookup
 *============================================================*/
int ec_known_sum(const char *expr_str, Expr **closed_form) {
    (void)expr_str;
    if (closed_form) *closed_form = NULL;
    return 0;
}
