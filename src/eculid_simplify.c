#include "eculid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Internal helpers
 *============================================================*/

static int exprs_equal(const Expr *a, const Expr *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case EC_NUM:   return a->num_val == b->num_val;
        case EC_VAR:   return a->var_name == b->var_name;
        default: {
            int l = exprs_equal(a->left, b->left);
            int r = exprs_equal(a->right, b->right);
            int g = exprs_equal(a->arg, b->arg);
            return l && r && g;
        }
    }
}

/* Recursively simplify children of a binary node */
static Expr* simplify_childs(Expr *e) {
    if (!e) return NULL;
    Expr *L = e->left  ? ec_simplify(e->left)  : NULL;
    Expr *R = e->right ? ec_simplify(e->right) : NULL;
    Expr *A = e->arg   ? ec_simplify(e->arg)   : NULL;
    Expr *C = e->cond  ? ec_simplify(e->cond)  : NULL;

    switch (e->type) {
        case EC_ADD: case EC_SUB: case EC_MUL: case EC_DIV:
        case EC_POW:
            return ec_binary(e->type, L, R);
        case EC_NEG: case EC_SIN: case EC_COS: case EC_TAN:
        case EC_COT: case EC_SEC: case EC_CSC: case EC_EXP:
        case EC_LN: case EC_LOG: case EC_LOG10: case EC_LOG2:
        case EC_SQRT: case EC_CBRT: case EC_ABS: case EC_EXP2:
        case EC_SINH: case EC_COSH: case EC_TANH: case EC_COTH:
        case EC_SECH: case EC_CSCH: case EC_ASIN: case EC_ACOS:
        case EC_ATAN: case EC_LN1P: case EC_EXPM1: case EC_FLOOR:
        case EC_CEIL: case EC_ROUND: case EC_TRUNC: case EC_SIGN:
        case EC_SERIES:
            return ec_unary(e->type, A);
        case EC_INT: case EC_DERIV: case EC_LIMIT:
            {
                Expr *node = ec_malloc(sizeof(Expr));
                *node = *e;
                node->left  = L;
                node->right = R;
                node->arg   = A;
                node->cond  = C;
                return node;
            }
        default:
            ec_free_expr(L); ec_free_expr(R); ec_free_expr(A); ec_free_expr(C);
            return ec_copy(e);
    }
}

/*============================================================
 * fold_constants — numeric constant folding for binary ops
 *============================================================*/
static Expr* fold_constants(Expr *e) {
    if (!e) return NULL;

    switch (e->type) {
        case EC_ADD: {
            Expr *L = fold_constants(e->left);
            Expr *R = fold_constants(e->right);
            if (L && L->type == EC_NUM && R && R->type == EC_NUM)
                return ec_num(L->num_val + R->num_val);
            if (L && L->type == EC_NUM && L->num_val == 0.0) { ec_free_expr(L); return R; }
            if (R && R->type == EC_NUM && R->num_val == 0.0) { ec_free_expr(R); return L; }
            return ec_binary(EC_ADD, L, R);
        }
        case EC_SUB: {
            Expr *L = fold_constants(e->left);
            Expr *R = fold_constants(e->right);
            if (L && L->type == EC_NUM && R && R->type == EC_NUM)
                return ec_num(L->num_val - R->num_val);
            if (R && R->type == EC_NUM && R->num_val == 0.0) { ec_free_expr(R); return L; }
            return ec_binary(EC_SUB, L, R);
        }
        case EC_MUL: {
            Expr *L = fold_constants(e->left);
            Expr *R = fold_constants(e->right);
            if (L && L->type == EC_NUM && R && R->type == EC_NUM)
                return ec_num(L->num_val * R->num_val);
            if ((L && L->type == EC_NUM && L->num_val == 0.0) ||
                (R && R->type == EC_NUM && R->num_val == 0.0)) {
                ec_free_expr(L); ec_free_expr(R); return ec_num(0);
            }
            if (L && L->type == EC_NUM && L->num_val == 1.0) { ec_free_expr(L); return R; }
            if (R && R->type == EC_NUM && R->num_val == 1.0) { ec_free_expr(R); return L; }
            if (L && L->type == EC_NUM && R && R->type == EC_NUM) {
                double v = L->num_val * R->num_val;
                ec_free_expr(L); ec_free_expr(R); return ec_num(v);
            }
            return ec_binary(EC_MUL, L, R);
        }
        case EC_DIV: {
            Expr *L = fold_constants(e->left);
            Expr *R = fold_constants(e->right);
            if (L && L->type == EC_NUM && R && R->type == EC_NUM && R->num_val != 0.0)
                return ec_num(L->num_val / R->num_val);
            if (L && L->type == EC_NUM && L->num_val == 0.0) { ec_free_expr(R); return ec_num(0); }
            if (R && R->type == EC_NUM && R->num_val == 1.0) { ec_free_expr(R); return L; }
            return ec_binary(EC_DIV, L, R);
        }
        case EC_POW: {
            Expr *L = fold_constants(e->left);
            Expr *R = fold_constants(e->right);
            if (L && L->type == EC_NUM && R && R->type == EC_NUM)
                return ec_num(pow(L->num_val, R->num_val));
            if (R && R->type == EC_NUM && R->num_val == 0.0) { ec_free_expr(L); ec_free_expr(R); return ec_num(1); }
            if (R && R->type == EC_NUM && R->num_val == 1.0) { ec_free_expr(R); return L; }
            if (L && L->type == EC_NUM && L->num_val == 1.0) { ec_free_expr(L); ec_free_expr(R); return ec_num(1); }
            if (L && L->type == EC_NUM && L->num_val == 0.0) { ec_free_expr(L); ec_free_expr(R); return ec_num(0); }
            return ec_binary(EC_POW, L, R);
        }
        case EC_NEG: {
            Expr *A = fold_constants(e->arg);
            if (A && A->type == EC_NUM) return ec_num(-A->num_val);
            return ec_unary(EC_NEG, A);
        }
        default:
            return ec_copy(e);
    }
}

/*============================================================
 * ec_simplify — main entry point
 *============================================================*/
Expr* ec_simplify(Expr *e) {
    if (!e) return NULL;

    /* 1. Recursively simplify children */
    Expr *simplified = simplify_childs(e);

    /* 2. Apply constant folding */
    Expr *folded = fold_constants(simplified);
    if (folded != simplified) ec_free_expr(simplified);

    /* 3. Recursively apply simplify again on result */
    Expr *recur = fold_constants(folded);
    if (recur != folded) ec_free_expr(folded);

    return recur ? recur : ec_copy(e);
}

/*============================================================
 * ec_simplify_full — full pipeline
 *============================================================*/
Expr* ec_simplify_full(Expr *e) {
    if (!e) return NULL;
    Expr *r = ec_simplify(e);
    r = ec_expand(r);
    Expr *t = ec_trig_reduce(r);
    ec_free_expr(r);
    Expr *l = ec_log_combine(t);
    ec_free_expr(t);
    Expr *s = fold_constants(l);
    ec_free_expr(l);
    return s;
}

/*============================================================
 * ec_expand — algebraic expansion: (a+b)*(c+d) = ac + ad + bc + bd
 *============================================================*/
Expr* ec_expand(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    switch (s->type) {
        case EC_MUL: {
            Expr *L = ec_expand(s->left);
            Expr *R = ec_expand(s->right);
            /* If either side is ADD/SUB, distribute */
            if ((L && (L->type == EC_ADD || L->type == EC_SUB)) &&
                (R && (R->type == EC_ADD || R->type == EC_SUB))) {
                /* (a+b)*(c+d) = ac + ad + bc + bd */
                Expr *ac = ec_binary(EC_MUL, ec_copy(L->left), ec_copy(R->left));
                Expr *ad = ec_binary(EC_MUL, ec_copy(L->left), ec_copy(R->right));
                Expr *bc = ec_binary(EC_MUL, ec_copy(L->right), ec_copy(R->left));
                Expr *bd = ec_binary(EC_MUL, ec_copy(L->right), ec_copy(R->right));
                Expr *t1 = ec_binary(EC_ADD, ac, ad);
                Expr *t2 = ec_binary(EC_ADD, bc, bd);
                Expr *result = ec_binary(EC_ADD, t1, t2);
                ec_free_expr(L); ec_free_expr(R);
                Expr *final = ec_expand(result);
                ec_free_expr(result);
                return final;
            } else if (L && (L->type == EC_ADD || L->type == EC_SUB)) {
                /* L*(R) where L is ADD/SUB: distribute */
                Expr *rl = ec_binary(EC_MUL, ec_copy(L->left), ec_copy(R));
                Expr *rr = ec_binary(EC_MUL, ec_copy(L->right), ec_copy(R));
                Expr *result = (L->type == EC_ADD)
                    ? ec_binary(EC_ADD, rl, rr)
                    : ec_binary(EC_SUB, rl, rr);
                ec_free_expr(L);
                Expr *final = ec_expand(result);
                ec_free_expr(result);
                return final;
            } else if (R && (R->type == EC_ADD || R->type == EC_SUB)) {
                /* (L)*R where R is ADD/SUB: distribute */
                Expr *rl = ec_binary(EC_MUL, ec_copy(L), ec_copy(R->left));
                Expr *rr = ec_binary(EC_MUL, ec_copy(L), ec_copy(R->right));
                Expr *result = (R->type == EC_ADD)
                    ? ec_binary(EC_ADD, rl, rr)
                    : ec_binary(EC_SUB, rl, rr);
                ec_free_expr(R);
                Expr *final = ec_expand(result);
                ec_free_expr(result);
                return final;
            } else {
                ec_free_expr(L); ec_free_expr(R);
                return s;
            }
        }
        case EC_ADD:
        case EC_SUB: {
            Expr *L = ec_expand(s->left);
            Expr *R = ec_expand(s->right);
            Expr *r = ec_binary(s->type, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_POW: {
            Expr *A = ec_expand(s->arg);
            ec_free_expr(s);
            return ec_unary(EC_POW, A);
        }
        case EC_NEG: {
            Expr *A = ec_expand(s->arg);
            ec_free_expr(s);
            return ec_unary(EC_NEG, A);
        }
        default:
            return s;
    }
}

Expr* ec_expand_trig(Expr *e) {
    return ec_expand(e);
}

/*============================================================
 * ec_factor — factor expressions: ac + ad = a*(c+d)
 *============================================================*/
static int var_in_expr(const Expr *e, char v) {
    if (!e) return 0;
    if (e->type == EC_VAR && e->var_name == v) return 1;
    if (e->type == EC_VAR) return 0;
    if (e->type == EC_NUM) return 0;
    return var_in_expr(e->left, v) || var_in_expr(e->right, v)
        || var_in_expr(e->arg, v);
}

static int var_power_in_expr(const Expr *e, char v) {
    if (!e) return 0;
    if (e->type == EC_POW && e->left && e->left->type == EC_VAR && e->left->var_name == v
        && e->right && e->right->type == EC_NUM)
        return (int)(e->right->num_val);
    if (e->type == EC_VAR && e->var_name == v) return 1;
    if (e->type == EC_NUM) return 0;
    return var_power_in_expr(e->left, v) + var_power_in_expr(e->right, v)
        + var_power_in_expr(e->arg, v);
}

static double coeff_in_term(const Expr *e) {
    if (!e) return 1.0;
    if (e->type == EC_MUL && e->left && e->left->type == EC_NUM)
        return e->left->num_val;
    if (e->type == EC_NUM) return e->num_val;
    return 1.0;
}

Expr* ec_factor(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    /* Factor ADD/SUB nodes */
    if (s->type == EC_ADD || s->type == EC_SUB) {
        Expr *L = ec_factor(s->left);
        Expr *R = ec_factor(s->right);

        /* Try to find common coefficient (numeric GCD) */
        double cL = coeff_in_term(L);
        double cR = coeff_in_term(R);
        int numeric_gcd = 0;
        double g = 0;
        if (cL != 0 && cR != 0 && cL == (int)cL && cR == (int)cR) {
            long a = (long)fabs(cL), b2 = (long)fabs(cR);
            while (b2) { long t = b2; b2 = a % b2; a = t; }
            g = a;
            if (g > 1) numeric_gcd = 1;
        }

        /* Try to find common variable power */
        int common_power = 0;
        char common_var = 0;
        for (char v = 'a'; v <= 'z'; v++) {
            int pL = var_power_in_expr(L, v);
            int pR = var_power_in_expr(R, v);
            if (pL > 0 && pR > 0) {
                int mn = pL < pR ? pL : pR;
                if (mn > common_power) { common_power = mn; common_var = v; }
            }
        }

        if (numeric_gcd || common_var) {
            Expr *factor_expr = NULL;
            if (numeric_gcd && common_var) {
                Expr *num_f = ec_num(g);
                Expr *var_f = common_power == 1
                    ? ec_varc(common_var)
                    : ec_binary(EC_POW, ec_varc(common_var), ec_num(common_power));
                factor_expr = ec_binary(EC_MUL, num_f, var_f);
            } else if (numeric_gcd) {
                factor_expr = ec_num(g);
            } else {
                factor_expr = common_power == 1
                    ? ec_varc(common_var)
                    : ec_binary(EC_POW, ec_varc(common_var), ec_num(common_power));
            }

            /* Build factored terms: divide each by factor */
            Expr *newL = NULL, *newR = NULL;

            if (numeric_gcd) {
                Expr *cL_e = ec_num(cL / g);
                Expr *cR_e = ec_num(cR / g);
                if (var_in_expr(L, common_var) && var_in_expr(R, common_var)) {
                    Expr *v_pow = common_power == 1
                        ? ec_varc(common_var)
                        : ec_binary(EC_POW, ec_varc(common_var), ec_num(common_power));
                    newL = ec_binary(EC_MUL, cL_e, ec_simplify(v_pow));
                    newR = ec_binary(EC_MUL, cR_e, ec_simplify(v_pow));
                } else {
                    newL = cL_e; newR = cR_e;
                }
            } else {
                /* Only variable common factor */
                Expr *var_pow = common_power == 1
                    ? ec_varc(common_var)
                    : ec_binary(EC_POW, ec_varc(common_var), ec_num(common_power));
                if (exprs_equal(L, var_pow)) {
                    newL = ec_num(1);
                } else if (L->type == EC_MUL) {
                    newL = ec_binary(EC_MUL, ec_copy(L->left), ec_copy(L->right));
                } else {
                    newL = ec_copy(L);
                }
                if (exprs_equal(R, var_pow)) {
                    newR = ec_num(1);
                } else if (R->type == EC_MUL) {
                    newR = ec_binary(EC_MUL, ec_copy(R->left), ec_copy(R->right));
                } else {
                    newR = ec_copy(R);
                }
                ec_free_expr(var_pow);
            }

            Expr *inner = (s->type == EC_ADD)
                ? ec_binary(EC_ADD, newL ? newL : ec_copy(L), newR ? newR : ec_copy(R))
                : ec_binary(EC_SUB, newL ? newL : ec_copy(L), newR ? newR : ec_copy(R));
            Expr *result = ec_binary(EC_MUL, factor_expr, ec_factor(inner));
            ec_free_expr(inner);
            ec_free_expr(L); ec_free_expr(R);
            ec_free_expr(s);
            return result;
        }

        Expr *r = ec_binary(s->type, L, R);
        ec_free_expr(s);
        return r;
    }

    if (s->type == EC_MUL) {
        Expr *L = ec_factor(s->left);
        Expr *R = ec_factor(s->right);
        Expr *r = ec_binary(EC_MUL, L, R);
        ec_free_expr(s);
        return r;
    }

    if (s->type == EC_POW) {
        Expr *A = ec_factor(s->arg);
        Expr *r = ec_binary(EC_POW, A, ec_copy(s->right));
        ec_free_expr(s);
        return r;
    }

    return s;
}

Expr* ec_factor_int(long n) {
    if (n <= 1) return ec_num(n);
    Expr *result = NULL;
    long p = 2;
    while (p * p <= n) {
        if (n % p == 0) {
            Expr *factor = ec_binary(EC_MUL,
                result ? result : ec_num(1),
                ec_num(p));
            ec_free_expr(result);
            result = factor;
            n /= p;
        } else {
            p++;
        }
    }
    if (n > 1) {
        Expr *factor = ec_binary(EC_MUL,
            result ? result : ec_num(1),
            ec_num(n));
        ec_free_expr(result);
        result = factor;
    }
    return result ? result : ec_num(1);
}

/*============================================================
 * ec_collect — collect like terms in variable v
 *============================================================*/
Expr* ec_collect(Expr *e, char var) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    if (s->type == EC_ADD || s->type == EC_SUB) {
        /* Collect terms: flatten ADD/SUB into a list, group by variable power */
        /* Simple approach: collect coefficients of same variable pattern */
        Expr *L = ec_collect(s->left, var);
        Expr *R = ec_collect(s->right, var);

        if (L && R &&
            L->type == EC_MUL && L->right && L->right->type == EC_POW &&
            L->right->left && L->right->left->type == EC_VAR && L->right->left->var_name == var &&
            R->type == EC_MUL && R->right && R->right->type == EC_POW &&
            R->right->left && R->right->left->type == EC_VAR && R->right->left->var_name == var &&
            exprs_equal(L->right->right, R->right->right)) {
            /* Same power of var: add coefficients */
            Expr *cL = L->left && L->left->type == EC_NUM ? L->left : ec_num(1);
            Expr *cR = R->left && R->left->type == EC_NUM ? R->left : ec_num(1);
            Expr *coeff = ec_num(cL->num_val + cR->num_val);
            Expr *var_part = ec_copy(L->right);
            Expr *result = ec_binary(EC_MUL, coeff, var_part);
            ec_free_expr(L); ec_free_expr(R);
            Expr *r = ec_binary(s->type, result, ec_num(0));
            ec_free_expr(s);
            return ec_collect(r, var);
        }
        Expr *r = ec_binary(s->type, L, R);
        ec_free_expr(s);
        return r;
    }

    return s;
}

/*============================================================
 * ec_combine — combine like terms
 *============================================================*/
Expr* ec_combine(Expr *e) {
    return ec_collect(e, 'x');
}

/*============================================================
 * ec_cancel — cancel common factors in DIV
 *============================================================*/
Expr* ec_cancel(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    if (s->type == EC_DIV) {
        if (exprs_equal(s->left, s->right)) {
            ec_free_expr(s);
            return ec_num(1);
        }
        /* Recursively cancel children */
        Expr *n = ec_cancel(s->left);
        Expr *d = ec_cancel(s->right);
        if (exprs_equal(n, d)) { ec_free_expr(n); ec_free_expr(d); ec_free_expr(s); return ec_num(1); }
        Expr *r = ec_binary(EC_DIV, n, d);
        ec_free_expr(s);
        return r;
    }

    return s;
}

/*============================================================
 * ec_normalize — bring expression to canonical form
 *============================================================*/
Expr* ec_normalize(Expr *e) {
    if (!e) return NULL;
    return ec_simplify_full(e);
}

/*============================================================
 * ec_rationalize — rationalize denominators
 *============================================================*/
Expr* ec_rationalize(Expr *e) {
    if (!e) return NULL;
    return ec_simplify_full(e);
}

/*============================================================
 * ec_trig_reduce — trigonometric identities
 *============================================================*/
Expr* ec_trig_reduce(Expr *e) {
    if (!e) return NULL;

    Expr *s = ec_simplify(e);

    switch (s->type) {
        case EC_ADD: {
            Expr *L = ec_trig_reduce(s->left);
            Expr *R = ec_trig_reduce(s->right);
            /* sin^2(x) + cos^2(x) = 1 */
            if (L && R) {
                /* Pattern: sin^2(x) + cos^2(x) */
                Expr *sin_sq = NULL, *cos_sq = NULL, *sin_arg = NULL, *cos_arg = NULL;
                if (L->type == EC_POW && L->left && L->left->type == EC_SIN
                    && L->right && L->right->type == EC_NUM && L->right->num_val == 2.0) {
                    sin_sq = L; sin_arg = L->left->arg;
                }
                if (L->type == EC_POW && L->left && L->left->type == EC_COS
                    && L->right && L->right->type == EC_NUM && L->right->num_val == 2.0) {
                    cos_sq = L; cos_arg = L->left->arg;
                }
                if (R->type == EC_POW && R->left && R->left->type == EC_SIN
                    && R->right && R->right->type == EC_NUM && R->right->num_val == 2.0) {
                    sin_sq = R; sin_arg = R->left->arg;
                }
                if (R->type == EC_POW && R->left && R->left->type == EC_COS
                    && R->right && R->right->type == EC_NUM && R->right->num_val == 2.0) {
                    cos_sq = R; cos_arg = R->left->arg;
                }
                if (sin_sq && cos_sq && sin_arg && cos_arg && exprs_equal(sin_arg, cos_arg)) {
                    ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                    return ec_num(1);
                }
            }
            Expr *r = ec_binary(EC_ADD, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_DIV: {
            Expr *n = ec_trig_reduce(s->left);
            Expr *d = ec_trig_reduce(s->right);
            /* sin(x) / cos(x) = tan(x) */
            if (n && d &&
                n->type == EC_POW && n->left && n->left->type == EC_SIN
                && n->right && n->right->type == EC_NUM && n->right->num_val == 1.0 &&
                d->type == EC_POW && d->left && d->left->type == EC_COS
                && d->right && d->right->type == EC_NUM && d->right->num_val == 1.0 &&
                exprs_equal(n->left->arg, d->left->arg)) {
                ec_free_expr(n); ec_free_expr(d); ec_free_expr(s);
                return ec_unary(EC_TAN, n->left->arg);
            }
            /* tan(x) = sin(x)/cos(x) — already simplified */
            Expr *r = ec_binary(EC_DIV, n, d);
            ec_free_expr(s);
            return r;
        }
        case EC_MUL: {
            Expr *L = ec_trig_reduce(s->left);
            Expr *R = ec_trig_reduce(s->right);
            Expr *r = ec_binary(EC_MUL, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_POW: {
            Expr *A = ec_trig_reduce(s->arg);
            Expr *r = ec_binary(EC_POW, A, ec_copy(s->right));
            ec_free_expr(s);
            return r;
        }
        case EC_NEG: {
            Expr *A = ec_trig_reduce(s->arg);
            Expr *r = ec_unary(EC_NEG, A);
            ec_free_expr(s);
            return r;
        }
        default:
            return s;
    }
}

Expr* ec_trig_expand(Expr *e) {
    return ec_expand(e);
}

Expr* ec_exp_to_trig(Expr *e) {
    return ec_simplify(e);
}

Expr* ec_trig_to_exp(Expr *e) {
    return ec_simplify(e);
}

/*============================================================
 * ec_power_simplify — simplify powers
 *============================================================*/
Expr* ec_power_simplify(Expr *e) {
    return ec_simplify(e);
}

/*============================================================
 * ec_log_combine — logarithmic identities
 *============================================================*/
static int is_log_of(const Expr *e, ECNodeType *out_base) {
    if (!e) return 0;
    if (e->type == EC_LN) { if (out_base) *out_base = EC_LN; return 1; }
    if (e->type == EC_LOG10) { if (out_base) *out_base = EC_LOG10; return 1; }
    if (e->type == EC_LOG2) { if (out_base) *out_base = EC_LOG2; return 1; }
    return 0;
}

Expr* ec_log_combine(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    switch (s->type) {
        case EC_ADD: {
            Expr *L = ec_log_combine(s->left);
            Expr *R = ec_log_combine(s->right);
            /* ln(a) + ln(b) = ln(a*b) */
            ECNodeType bL = 0, bR = 0;
            if (is_log_of(L, &bL) && is_log_of(R, &bR) && bL == bR) {
                Expr *arg = ec_binary(EC_MUL, ec_copy(L->arg), ec_copy(R->arg));
                ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                return ec_log_combine(ec_unary(bL, arg));
            }
            /* n*ln(a) + ln(b) = ln(a^n * b) */
            if (L->type == EC_MUL && L->right && is_log_of(L->right, &bL) &&
                is_log_of(R, &bR) && bL == bR) {
                Expr *ln_node = ec_unary(bL, ec_copy(L->right->arg));
                Expr *combined = ec_binary(EC_MUL, ec_copy(L->left), ln_node);
                Expr *arg = ec_binary(EC_MUL, combined, ec_copy(R->arg));
                Expr *result = ec_unary(bL, arg);
                ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                return ec_log_combine(result);
            }
            Expr *r = ec_binary(EC_ADD, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_SUB: {
            Expr *L = ec_log_combine(s->left);
            Expr *R = ec_log_combine(s->right);
            /* ln(a) - ln(b) = ln(a/b) */
            ECNodeType bL = 0, bR = 0;
            if (is_log_of(L, &bL) && is_log_of(R, &bR) && bL == bR) {
                Expr *arg = ec_binary(EC_DIV, ec_copy(L->arg), ec_copy(R->arg));
                ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                return ec_log_combine(ec_unary(bL, arg));
            }
            Expr *r = ec_binary(EC_SUB, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_MUL: {
            Expr *L = ec_log_combine(s->left);
            Expr *R = ec_log_combine(s->right);
            /* n * ln(a) = ln(a^n) */
            ECNodeType bR = 0;
            if (L && L->type == EC_NUM && is_log_of(R, &bR)) {
                Expr *arg = ec_binary(EC_POW, ec_copy(R->arg), ec_copy(L));
                ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                return ec_log_combine(ec_unary(bR, arg));
            }
            /* ln(a) * n = ln(a^n) */
            ECNodeType bL = 0;
            if (R && R->type == EC_NUM && is_log_of(L, &bL)) {
                Expr *arg = ec_binary(EC_POW, ec_copy(L->arg), ec_copy(R));
                ec_free_expr(L); ec_free_expr(R); ec_free_expr(s);
                return ec_log_combine(ec_unary(bL, arg));
            }
            Expr *r = ec_binary(EC_MUL, L, R);
            ec_free_expr(s);
            return r;
        }
        case EC_POW: {
            Expr *A = ec_log_combine(s->arg);
            Expr *exp = ec_copy(s->right);
            /* ln(a)^n — can't simplify unless n is known */
            Expr *r = ec_binary(EC_POW, A, exp);
            ec_free_expr(s);
            return r;
        }
        case EC_NEG: {
            Expr *A = ec_log_combine(s->arg);
            Expr *r = ec_unary(EC_NEG, A);
            ec_free_expr(s);
            return r;
        }
        default:
            return s;
    }
}

Expr* ec_log_expand(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);

    /* Expand ln(a*b) = ln(a) + ln(b) */
    if (s->type == EC_LN && s->arg && s->arg->type == EC_MUL) {
        Expr *a = ec_copy(s->arg->left);
        Expr *b = ec_copy(s->arg->right);
        Expr *la = ec_unary(EC_LN, ec_log_expand(a));
        Expr *lb = ec_unary(EC_LN, ec_log_expand(b));
        ec_free_expr(s);
        return ec_binary(EC_ADD, la, lb);
    }
    /* Expand ln(a^n) = n*ln(a) */
    if (s->type == EC_LN && s->arg && s->arg->type == EC_POW) {
        Expr *base = ec_copy(s->arg->left);
        Expr *exp = ec_copy(s->arg->right);
        Expr *ln_b = ec_unary(EC_LN, ec_log_expand(base));
        ec_free_expr(s);
        return ec_binary(EC_MUL, exp, ln_b);
    }
    /* Expand ln(a/b) = ln(a) - ln(b) */
    if (s->type == EC_LN && s->arg && s->arg->type == EC_DIV) {
        Expr *a = ec_copy(s->arg->left);
        Expr *b = ec_copy(s->arg->right);
        Expr *la = ec_unary(EC_LN, ec_log_expand(a));
        Expr *lb = ec_unary(EC_LN, ec_log_expand(b));
        ec_free_expr(s);
        return ec_binary(EC_SUB, la, lb);
    }

    if (s->type == EC_MUL) {
        Expr *L = ec_log_expand(s->left);
        Expr *R = ec_log_expand(s->right);
        Expr *r = ec_binary(EC_MUL, L, R);
        ec_free_expr(s);
        return r;
    }
    if (s->type == EC_POW) {
        Expr *A = ec_log_expand(s->arg);
        Expr *r = ec_binary(EC_POW, A, ec_copy(s->right));
        ec_free_expr(s);
        return r;
    }
    if (s->type == EC_NEG) {
        Expr *A = ec_log_expand(s->arg);
        Expr *r = ec_unary(EC_NEG, A);
        ec_free_expr(s);
        return r;
    }
    return s;
}

/*============================================================
 * ec_abs_simplify — simplify absolute values
 *============================================================*/
Expr* ec_abs_simplify(Expr *e) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);
    if (s->type == EC_ABS && s->arg) {
        if (s->arg->type == EC_NUM) {
            double v = s->arg->num_val;
            ec_free_expr(s);
            return ec_num(v < 0 ? -v : v);
        }
    }
    return s;
}

/*============================================================
 * ec_rewrite — change node type, preserve meaning
 *============================================================*/
Expr* ec_rewrite(Expr *e, ECNodeType new_type) {
    if (!e) return NULL;
    Expr *s = ec_simplify(e);
    switch (s->type) {
        case EC_ADD: case EC_SUB:
        case EC_MUL: case EC_DIV:
        case EC_POW:
            return ec_binary(new_type, ec_copy(s->left), ec_copy(s->right));
        case EC_NEG: case EC_SIN: case EC_COS: case EC_TAN:
        case EC_LN: case EC_EXP: case EC_ABS: case EC_SQRT:
        case EC_SINH: case EC_COSH: case EC_TANH:
            return ec_unary(new_type, ec_copy(s->arg));
        default:
            return ec_copy(s);
    }
}

/*============================================================
 * ec_is_simplified — returns 1 if no simplifiable patterns
 *============================================================*/
int ec_is_simplified(const Expr *e) {
    if (!e) return 1;

    switch (e->type) {
        case EC_ADD:
        case EC_SUB: {
            if (e->left && e->left->type == EC_NUM && e->left->num_val == 0.0) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 0.0) return 0;
            return ec_is_simplified(e->left) && ec_is_simplified(e->right);
        }
        case EC_MUL: {
            if (e->left && e->left->type == EC_NUM && e->left->num_val == 0.0) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 0.0) return 0;
            if (e->left && e->left->type == EC_NUM && e->left->num_val == 1.0) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 1.0) return 0;
            return ec_is_simplified(e->left) && ec_is_simplified(e->right);
        }
        case EC_DIV: {
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 1.0) return 0;
            return ec_is_simplified(e->left) && ec_is_simplified(e->right);
        }
        case EC_POW: {
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 0.0) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val == 1.0) return 0;
            if (e->left && e->left->type == EC_NUM && e->left->num_val == 1.0) return 0;
            return ec_is_simplified(e->left) && ec_is_simplified(e->right);
        }
        case EC_NEG: {
            if (e->arg && e->arg->type == EC_NUM) return 0;
            return ec_is_simplified(e->arg);
        }
        default:
            return ec_is_simplified(e->arg);
    }
}

/*============================================================
 * ec_is_polynomial — check if expression is polynomial in var
 *============================================================*/
int ec_is_polynomial(const Expr *e, const char *var) {
    if (!e) return 1;
    char v = var ? var[0] : 0;

    switch (e->type) {
        case EC_NUM: return 1;
        case EC_VAR:
            return (v == 0) || (e->var_name == v);
        case EC_ADD: case EC_SUB:
            return ec_is_polynomial(e->left, var) && ec_is_polynomial(e->right, var);
        case EC_MUL: {
            if (!ec_is_polynomial(e->left, var) || !ec_is_polynomial(e->right, var))
                return 0;
            /* Both factors must be polynomials in var, no cross-products of vars */
            if (e->left->type == EC_VAR && e->right->type == EC_VAR && v != 0)
                return 0;
            return 1;
        }
        case EC_DIV: {
            /* Allow rational polynomials (numerator/denominator must be const in var) */
            if (!ec_is_polynomial(e->left, var)) return 0;
            /* Denominator must not contain the variable */
            return !ec_has_var(e->right, v);
        }
        case EC_POW: {
            if (!ec_is_polynomial(e->left, var)) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val >= 0
                && e->right->num_val == (int)e->right->num_val)
                return 1;
            return 0;
        }
        case EC_NEG:
            return ec_is_polynomial(e->arg, var);
        default:
            return 0;
    }
}

/*============================================================
 * ec_is_rational — check if expression is rational in var
 *============================================================*/
int ec_is_rational(const Expr *e, const char *var) {
    if (!e) return 1;
    char v = var ? var[0] : 0;

    switch (e->type) {
        case EC_NUM: return 1;
        case EC_VAR:
            return (v == 0) || (e->var_name == v);
        case EC_ADD: case EC_SUB:
            return ec_is_rational(e->left, var) && ec_is_rational(e->right, var);
        case EC_MUL: case EC_DIV:
            return ec_is_rational(e->left, var) && ec_is_rational(e->right, var);
        case EC_POW: {
            if (!ec_is_rational(e->left, var)) return 0;
            if (e->right && e->right->type == EC_NUM && e->right->num_val >= 0
                && e->right->num_val == (int)e->right->num_val)
                return 1;
            return 0;
        }
        case EC_NEG:
            return ec_is_rational(e->arg, var);
        default:
            return 0;
    }
}

/*============================================================
 * ec_polynomial_div — polynomial long division
 *   (numerator) / (denominator) = quotient + remainder/denominator
 *============================================================*/

/* Get the degree of expr in variable var, assuming expr is a polynomial */
static int poly_degree(const Expr *e, char var) {
    if (!e) return -1;
    if (e->type == EC_NUM) return 0;
    if (e->type == EC_VAR) return (e->var_name == var) ? 1 : 0;
    if (e->type == EC_POW && e->left && e->left->type == EC_VAR && e->left->var_name == var
        && e->right && e->right->type == EC_NUM && e->right->num_val >= 0
        && e->right->num_val == (int)e->right->num_val) {
        return (int)e->right->num_val;
    }
    if (e->type == EC_ADD || e->type == EC_SUB) {
        int dL = poly_degree(e->left, var);
        int dR = poly_degree(e->right, var);
        return dL > dR ? dL : dR;
    }
    if (e->type == EC_MUL) {
        return poly_degree(e->left, var) + poly_degree(e->right, var);
    }
    if (e->type == EC_NEG) return poly_degree(e->arg, var);
    return 0;
}

/* Get the leading coefficient of e with respect to var */
static double lead_coeff(const Expr *e, char var) {
    if (!e) return 0.0;
    if (e->type == EC_NUM) return e->num_val;
    if (e->type == EC_VAR) return (e->var_name == var) ? 1.0 : 0.0;
    if (e->type == EC_POW && e->left && e->left->type == EC_VAR && e->left->var_name == var
        && e->right && e->right->type == EC_NUM) {
        return 1.0;
    }
    if (e->type == EC_ADD || e->type == EC_SUB) {
        int dL = poly_degree(e->left, var);
        int dR = poly_degree(e->right, var);
        return dL >= dR ? lead_coeff(e->left, var) : lead_coeff(e->right, var);
    }
    if (e->type == EC_MUL) {
        double cL = lead_coeff(e->left, var);
        double cR = lead_coeff(e->right, var);
        if (cL != 0.0) return cL * cR;
        return cR * lead_coeff(e->left, var);
    }
    if (e->type == EC_NEG) return -lead_coeff(e->arg, var);
    return 0.0;
}

int ec_polynomial_div(Expr *num, Expr *den, const char *var,
                      Expr **quotient, Expr **remainder) {
    if (!num || !den || !var || !quotient || !remainder) {
        if (quotient) *quotient = NULL;
        if (remainder) *remainder = NULL;
        return 0;
    }

    char v = var[0];

    /* Check that denominator is not zero */
    if (ec_is_zero(den)) {
        *quotient = NULL; *remainder = NULL;
        return 0;
    }

    /* Check both are polynomials */
    if (!ec_is_polynomial(num, var) || !ec_is_polynomial(den, var)) {
        *quotient = ec_num(0);
        *remainder = ec_copy(num);
        return 0;
    }

    /* If num degree < den degree, quotient = 0, remainder = num */
    int deg_num = poly_degree(num, v);
    int deg_den = poly_degree(den, v);
    if (deg_num < deg_den) {
        *quotient = ec_num(0);
        *remainder = ec_copy(num);
        return 1;
    }

    /* Long division algorithm */
    Expr *Q = ec_num(0);   /* quotient accumulator */
    Expr *R = ec_copy(num); /* remainder */
    double d_coeff = lead_coeff(den, v);
    int d_deg = poly_degree(den, v);

    while (R && !ec_is_zero(R) && poly_degree(R, v) >= d_deg) {
        double r_coeff = lead_coeff(R, v);
        int r_deg = poly_degree(R, v);

        /* quotient term = (r_coeff / d_coeff) * x^(r_deg - d_deg) */
        Expr *term_coeff = ec_num(r_coeff / d_coeff);
        Expr *term;
        if (r_deg - d_deg == 0) {
            term = term_coeff;
        } else if (r_deg - d_deg == 1) {
            term = ec_binary(EC_MUL, term_coeff, ec_varc(v));
        } else {
            Expr *pow_term = ec_binary(EC_POW, ec_varc(v), ec_num(r_deg - d_deg));
            term = ec_binary(EC_MUL, term_coeff, pow_term);
        }

        /* Q = Q + term */
        Expr *newQ = ec_binary(EC_ADD, Q, term);
        Q = newQ;

        /* R = R - term * den */
        Expr *sub = ec_binary(EC_MUL, ec_copy(term), ec_copy(den));
        Expr *newR = ec_binary(EC_SUB, R, sub);
        R = ec_simplify(newR);
        ec_free_expr(sub);
    }

    *quotient = Q;
    *remainder = R;
    return 1;
}
