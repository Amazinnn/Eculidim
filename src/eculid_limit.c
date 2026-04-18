#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Internal helpers
 *============================================================*/
static double eval_point(const Expr *e, const char *var, double val) {
    if (!e) return 0.0;
    ECEnv env = ec_env_new();
    char buf[2] = {var[0], 0};
    ec_env_bind(&env, buf, val);
    ECValue v = ec_eval(e, &env);
    double r = ec_value_as_real(&v);
    ec_value_free(&v);
    ec_env_free(&env);
    return r;
}

static int is_inf(double v) { return isinf(v); }
static int is_indet(double v) { return v != v; }
static int close_to(double a, double b, double eps) { return fabs(a - b) < eps; }

/*============================================================
 * ec_limit — main entry
 *============================================================
 * ec_limit — evaluate lim_{var -> point} e
 * Strategy:
 *   1. Direct substitution
 *   2. If 0/0 or inf/inf: simplify expression (factor, rationalize) and retry
 *   3. Left/right limit for vertical asymptotes (1/x at 0)
 *============================================================*/
Expr* ec_limit(Expr *e, char var, Expr *point, ECLimitDir dir) {
    if (!e) return NULL;
    char buf[2] = {var, 0};

    /* Evaluate the limit point */
    double pt_val = 0.0;
    int pt_is_const = 0;
    if (point && point->type == EC_NUM) {
        pt_val = point->num_val;
        pt_is_const = 1;
    } else if (point && point->type == EC_INF) {
        pt_val = 1e300;
    } else if (point && point->type == EC_NINF) {
        pt_val = -1e300;
    } else if (point && point->type == EC_VAR) {
        /* Symbolic point: can't evaluate numerically */
        /* Try to evaluate symbolically */
        ECValue pv = ec_eval_sym(point);
        if (pv.type == EC_VAL_REAL) { pt_val = pv.real; pt_is_const = 1; }
        ec_value_free(&pv);
    } else {
        ECValue pv = ec_eval(point, NULL);
        if (pv.type == EC_VAL_REAL) { pt_val = pv.real; pt_is_const = 1; }
        ec_value_free(&pv);
    }

    if (!pt_is_const) {
        /* Symbolic point: try symbolic limit for common patterns */
        return ec_limit_node(ec_copy(e), var, point ? ec_copy(point) : NULL);
    }

    /* ---- Approach from left or right for finite points ---- */
    if (dir == EC_LIMIT_LEFT || dir == EC_LIMIT_RIGHT) {
        double h = (dir == EC_LIMIT_LEFT) ? -1e-8 : 1e-8;
        double val = eval_point(e, buf, pt_val + h);
        if (!is_indet(val) && !is_inf(val))
            return ec_num(val);
        return NULL;
    }

    /* ---- Default: try both sides first ---- */
    double lval = eval_point(e, buf, pt_val - 1e-8);
    double rval = eval_point(e, buf, pt_val + 1e-8);

    /* If both sides give same finite answer, that's the limit */
    if (!is_indet(lval) && !is_inf(lval) &&
        !is_indet(rval) && !is_inf(rval) &&
        close_to(lval, rval, 1e-6)) {
        return ec_num((lval + rval) * 0.5);
    }

    /* ---- Try direct substitution at the point ---- */
    double direct = eval_point(e, buf, pt_val);
    if (!is_indet(direct) && !is_inf(direct))
        return ec_num(direct);

    /* ---- Handle specific indeterminate forms ---- */
    /* For polynomials: factor and substitute */
    Expr *s = ec_simplify(e);
    double direct2 = eval_point(s, buf, pt_val);
    if (!is_indet(direct2) && !is_inf(direct2)) {
        Expr *r = ec_num(direct2);
        ec_free_expr(s);
        return r;
    }
    ec_free_expr(s);

    /* ---- Try expansion around the point ---- */
    /* Taylor series approach for smooth functions */
    Expr *taylor = ec_taylor(ec_copy(e), buf, ec_num(pt_val), 10);
    if (taylor) {
        double tval = eval_point(taylor, buf, pt_val);
        if (!is_indet(tval) && !is_inf(tval)) {
            Expr *r = ec_num(tval);
            ec_free_expr(taylor);
            return r;
        }
        ec_free_expr(taylor);
    }

    /* ---- Limit at infinity ---- */
    if (pt_is_const && fabs(pt_val) > 1e200) {
        /* For rational functions: substitute large value */
        if (e->type == EC_DIV) {
            double large_val = eval_point(e, buf, (pt_val > 0) ? 1e10 : -1e10);
            if (!is_indet(large_val))
                return ec_num(large_val);
        }
        /* For x->inf of a^(-x) type: tends to 0 */
        if (e->type == EC_POW && e->right && e->right->type == EC_VAR
            && e->right->var_name == var) {
            if (e->left && e->left->type == EC_NUM && e->left->num_val > 1)
                return ec_num(0);
        }
    }

    /* Could not determine limit */
    return ec_limit_node(ec_copy(e), var, point ? ec_copy(point) : NULL);
}

Expr* ec_limit_at_inf(Expr *e, char var, int sign) {
    if (!e) return NULL;
    Expr *pt = sign > 0 ? ec_const(EC_INF) : ec_const(EC_NINF);
    Expr *r = ec_limit(e, var, pt, EC_LIMIT_BOTH);
    ec_free_expr(pt);
    (void)var;
    return r ? r : ec_limit_node(ec_copy(e), var, ec_const(sign > 0 ? EC_INF : EC_NINF));
}

/*============================================================
 * ec_limit_exists — check if two-sided limit exists
 *============================================================*/
int ec_limit_exists(Expr *e, char var, Expr *point) {
    if (!e) return 0;
    char buf[2] = {var, 0};

    double pt_val = 0.0;
    int pt_is_const = 0;
    if (point && point->type == EC_NUM) {
        pt_val = point->num_val; pt_is_const = 1;
    } else if (point && (point->type == EC_INF || point->type == EC_NINF)) {
        return 0; /* limit at infinity: handled separately */
    } else {
        ECValue pv = ec_eval(point, NULL);
        if (pv.type == EC_VAL_REAL) { pt_val = pv.real; pt_is_const = 1; }
        ec_value_free(&pv);
    }
    if (!pt_is_const) return 0;

    double lval = eval_point(e, buf, pt_val - 1e-8);
    double rval = eval_point(e, buf, pt_val + 1e-8);

    if (is_indet(lval) || is_inf(lval)) return 0;
    if (is_indet(rval) || is_inf(rval)) return 0;
    return close_to(lval, rval, 1e-6);
}

/*============================================================
 * ec_limit_with_steps — limit with derivation steps
 *============================================================*/
Derivation* ec_limit_with_steps(Expr *e, char var, Expr *point) {
    Derivation *d = ec_derivation_new();
    char buf[512];
    char *orig = ec_to_latex(e);
    char varbuf[2] = {var, 0};
    char *pt_str = point ? ec_to_latex(point) : ec_strdup("0");
    snprintf(buf, sizeof(buf), "\\lim_{%s \\to %s} %s", varbuf, pt_str, orig);
    ec_derivation_add(d, buf, "Initial Limit");
    ec_free(orig); ec_free(pt_str);

    Expr *result = ec_limit(e, var, point, EC_LIMIT_BOTH);
    if (result) {
        char *res = ec_to_latex(result);
        if (result->type != EC_LIMIT) {
            snprintf(buf, sizeof(buf), "= %s", res);
        } else {
            snprintf(buf, sizeof(buf), "%s \\quad \\text{(limit not resolved)}", res);
        }
        ec_derivation_add(d, buf, "Direct Substitution / Simplification");
        ec_free(res);
        ec_free_expr(result);
    } else {
        ec_derivation_add(d, "\\text{Unable to compute limit symbolically}", "No Rule");
    }
    return d;
}
