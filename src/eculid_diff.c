#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * Internal recursive differentiator with step recording
 *============================================================*/
static Expr* diff_rec(const Expr *e, char var, Derivation *steps) {
    if (!e) return NULL;

    switch (e->type) {
        /*---- Constants and Variables ----*/
        case EC_NUM:
            /* Constant Rule: d(c)/dx = 0 */
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = 0", orig);
                ec_derivation_add(steps, buf, "Constant Rule");
                ec_free(orig);
            }
            return ec_num(0);

        case EC_VAR:
            /* Variable Rule: d(x)/dx = 1, d(c)/dx = 0 */
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                if (e->var_name == var) {
                    snprintf(buf, sizeof(buf), "d/d%c[%s] = 1", var, orig);
                    ec_derivation_add(steps, buf, "Variable Rule");
                } else {
                    snprintf(buf, sizeof(buf), "d/d%c[%s] = 0  (constant w.r.t. %c)", var, orig, var);
                    ec_derivation_add(steps, buf, "Constant Rule");
                }
                ec_free(orig);
            }
            return e->var_name == var ? ec_num(1) : ec_num(0);

        /*---- Arithmetic Operators ----*/
        case EC_ADD: {
            Expr *left  = diff_rec(e->left, var, steps);
            Expr *right = diff_rec(e->right, var, steps);
            Expr *result = ec_binary(EC_ADD, left, right);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Addition Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_SUB: {
            Expr *left  = diff_rec(e->left, var, steps);
            Expr *right = diff_rec(e->right, var, steps);
            Expr *result = ec_binary(EC_SUB, left, right);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Subtraction Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_MUL: {
            /* Product Rule: d(uv)/dx = u'v + uv' */
            Expr *left  = diff_rec(e->left, var, steps);
            Expr *right = diff_rec(e->right, var, steps);
            Expr *term1 = ec_binary(EC_MUL, left, ec_copy(e->right));
            Expr *term2 = ec_binary(EC_MUL, ec_copy(e->left), right);
            Expr *result = ec_binary(EC_ADD, term1, term2);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Product Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_DIV: {
            /* Quotient Rule: d(u/v)/dx = (u'v - uv') / v^2 */
            Expr *u = e->left, *v = e->right;
            Expr *du = diff_rec(u, var, steps);
            Expr *dv = diff_rec(v, var, steps);
            Expr *num = ec_binary(EC_SUB,
                ec_binary(EC_MUL, du, ec_copy(v)),
                ec_binary(EC_MUL, ec_copy(u), dv));
            Expr *den = ec_binary(EC_POW, ec_copy(v), ec_num(2));
            Expr *result = ec_binary(EC_DIV, num, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Quotient Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_POW: {
            Expr *base = e->left, *exp = e->right;
            if (ec_is_const(exp)) {
                /* Power Rule: d(x^n)/dx = n*x^(n-1)*x' */
                Expr *np1 = ec_num(exp->num_val - 1);
                Expr *pow_term = ec_binary(EC_POW, ec_copy(base), np1);
                Expr *coeff    = ec_binary(EC_MUL, ec_copy(exp), pow_term);
                Expr *result   = ec_binary(EC_MUL, coeff, diff_rec(base, var, steps));
                if (steps) {
                    char *orig = ec_to_latex(e);
                    char *res  = ec_to_latex(result);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                    ec_derivation_add(steps, buf, "Power Rule");
                    ec_free(orig); ec_free(res);
                }
                return result;
            }
            if (ec_is_one(base)) {
                if (steps) {
                    char buf[512];
                    char *orig = ec_to_latex(e);
                    snprintf(buf, sizeof(buf), "d/dx[1^anything] = 0");
                    ec_derivation_add(steps, buf, "Constant Rule");
                    ec_free(orig);
                }
                return ec_num(0);
            }
            /* General: d(f^g)/dx = f^g * (g'*ln(f) + g*f'/f) */
            Expr *lnf    = ec_unary(EC_LN, ec_copy(base));
            Expr *g_prime = diff_rec(exp, var, steps);
            Expr *f_prime = diff_rec(base, var, steps);
            Expr *term1 = ec_binary(EC_MUL, g_prime, lnf);
            Expr *f_copy = ec_copy(base);
            Expr *term2 = ec_binary(EC_DIV,
                ec_binary(EC_MUL, ec_copy(exp), f_prime),
                f_copy);
            Expr *sum   = ec_binary(EC_ADD, term1, term2);
            Expr *result = ec_binary(EC_MUL, ec_copy(e), sum);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "General Power Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_NEG: {
            /* Chain Rule for negation: d(-u)/dx = -(u') */
            Expr *arg    = diff_rec(e->arg, var, steps);
            Expr *result = ec_unary(EC_NEG, arg);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Trigonometric Functions ----*/
        case EC_SIN: {
            /* d(sin(u))/dx = cos(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_COS, ec_copy(arg)), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_COS: {
            /* d(cos(u))/dx = -sin(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *sin_arg = ec_unary(EC_SIN, ec_copy(arg));
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_NEG, sin_arg), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_TAN: {
            /* d(tan(u))/dx = sec^2(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *cos_arg = ec_unary(EC_COS, ec_copy(arg));
            Expr *result  = ec_binary(EC_MUL,
                ec_binary(EC_POW, cos_arg, ec_num(-2)),
                arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_COT: {
            /* d(cot(u))/dx = -csc^2(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *csc_arg = ec_unary(EC_CSC, ec_copy(arg));
            Expr *neg_csc2 = ec_unary(EC_NEG, ec_binary(EC_POW, csc_arg, ec_num(2)));
            Expr *result  = ec_binary(EC_MUL, neg_csc2, arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_SEC: {
            /* d(sec(u))/dx = sec(u)*tan(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *sec_arg = ec_unary(EC_SEC, ec_copy(arg));
            Expr *tan_arg = ec_unary(EC_TAN, ec_copy(arg));
            Expr *result  = ec_binary(EC_MUL,
                ec_binary(EC_MUL, sec_arg, tan_arg),
                arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_CSC: {
            /* d(csc(u))/dx = -csc(u)*cot(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *csc_arg = ec_unary(EC_CSC, ec_copy(arg));
            Expr *cot_arg = ec_unary(EC_COT, ec_copy(arg));
            Expr *neg = ec_binary(EC_MUL, csc_arg, cot_arg);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_NEG, neg), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Inverse Trigonometric Functions ----*/
        case EC_ASIN: {
            /* d(asin(u))/dx = u' / sqrt(1 - u^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_unary(EC_SQRT, ec_binary(EC_SUB, ec_num(1), u2));
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ACOS: {
            /* d(acos(u))/dx = -u' / sqrt(1 - u^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_unary(EC_SQRT, ec_binary(EC_SUB, ec_num(1), u2));
            Expr *result  = ec_binary(EC_DIV, ec_unary(EC_NEG, arg_der), den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ATAN: {
            /* d(atan(u))/dx = u' / (1 + u^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_binary(EC_ADD, ec_num(1), u2);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ACOT: {
            /* d(acot(u))/dx = -u' / (1 + u^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_binary(EC_ADD, ec_num(1), u2);
            Expr *result  = ec_binary(EC_DIV, ec_unary(EC_NEG, arg_der), den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ASEC: {
            /* d(asec(u))/dx = u' / (|u|*sqrt(u^2-1)) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_SUB, u2, ec_num(1)));
            Expr *abs_u = ec_unary(EC_ABS, ec_copy(arg));
            Expr *den = ec_binary(EC_MUL, abs_u, sqrt_term);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ACSC: {
            /* d(acsc(u))/dx = -u' / (|u|*sqrt(u^2-1)) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_SUB, u2, ec_num(1)));
            Expr *abs_u = ec_unary(EC_ABS, ec_copy(arg));
            Expr *den = ec_binary(EC_MUL, abs_u, sqrt_term);
            Expr *result  = ec_binary(EC_DIV, ec_unary(EC_NEG, arg_der), den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Hyperbolic Functions ----*/
        case EC_SINH: {
            /* d(sinh(u))/dx = cosh(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_COSH, ec_copy(arg)), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_COSH: {
            /* d(cosh(u))/dx = sinh(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_SINH, ec_copy(arg)), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_TANH: {
            /* d(tanh(u))/dx = sech^2(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *sech_arg = ec_unary(EC_SECH, ec_copy(arg));
            Expr *result  = ec_binary(EC_MUL,
                ec_binary(EC_POW, sech_arg, ec_num(2)),
                arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_COTH: {
            /* d(coth(u))/dx = -csch^2(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *csch_arg = ec_unary(EC_CSCH, ec_copy(arg));
            Expr *neg_csch2 = ec_unary(EC_NEG, ec_binary(EC_POW, csch_arg, ec_num(2)));
            Expr *result  = ec_binary(EC_MUL, neg_csch2, arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_SECH: {
            /* d(sech(u))/dx = -sech(u)*tanh(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *sech_arg = ec_unary(EC_SECH, ec_copy(arg));
            Expr *tanh_arg = ec_unary(EC_TANH, ec_copy(arg));
            Expr *neg = ec_binary(EC_MUL, sech_arg, tanh_arg);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_NEG, neg), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_CSCH: {
            /* d(csch(u))/dx = -csch(u)*coth(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *csch_arg = ec_unary(EC_CSCH, ec_copy(arg));
            Expr *coth_arg = ec_unary(EC_COTH, ec_copy(arg));
            Expr *neg = ec_binary(EC_MUL, csch_arg, coth_arg);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_NEG, neg), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Inverse Hyperbolic Functions ----*/
        case EC_ASINH: {
            /* d(asinh(u))/dx = u' / sqrt(u^2+1) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_unary(EC_SQRT, ec_binary(EC_ADD, u2, ec_num(1)));
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ACOSH: {
            /* d(acosh(u))/dx = u' / sqrt(u^2-1) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_unary(EC_SQRT, ec_binary(EC_SUB, u2, ec_num(1)));
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_ATANH: {
            /* d(atanh(u))/dx = u' / (1 - u^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *den = ec_binary(EC_SUB, ec_num(1), u2);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Exponential and Logarithmic Functions ----*/
        case EC_EXP: {
            /* d(e^u)/dx = e^u * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_MUL, ec_copy(e), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_LN: {
            /* d(ln(u))/dx = u' / u */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_DIV, arg_der, ec_copy(arg));
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_LOG: {
            /* d(log(u))/dx = u' / (u * ln(10)) for log10, u'/u for natural log */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *ln10 = ec_unary(EC_LN, ec_const(EC_E));
            Expr *den = ec_binary(EC_MUL, ec_copy(arg), ln10);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_LOG10: {
            /* d(log10(u))/dx = u' / (u * ln(10)) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *ln10 = ec_unary(EC_LN, ec_const(EC_E));
            Expr *den = ec_binary(EC_MUL, ec_copy(arg), ln10);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_LOG2: {
            /* d(log2(u))/dx = u' / (u * ln(2)) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *ln2 = ec_unary(EC_LN, ec_num(2));
            Expr *den = ec_binary(EC_MUL, ec_copy(arg), ln2);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Root Functions ----*/
        case EC_SQRT: {
            /* d(sqrt(u))/dx = u' / (2*sqrt(u)) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *two_root = ec_binary(EC_MUL, ec_num(2), ec_copy(e));
            Expr *result  = ec_binary(EC_DIV, arg_der, two_root);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_CBRT: {
            /* d(cbrt(u))/dx = u' / (3 * cbrt(u)^2) */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *root_sq = ec_binary(EC_POW, ec_copy(e), ec_num(2));
            Expr *den = ec_binary(EC_MUL, ec_num(3), root_sq);
            Expr *result  = ec_binary(EC_DIV, arg_der, den);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Absolute Value and Sign ----*/
        case EC_ABS: {
            /* d(|u|)/dx = sign(u) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result  = ec_binary(EC_MUL, ec_unary(EC_SIGN, ec_copy(arg)), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Piecewise-constant functions ----*/
        case EC_FLOOR:
        case EC_CEIL:
        case EC_ROUND:
        case EC_TRUNC: {
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = 0  (piecewise constant)", orig);
                ec_derivation_add(steps, buf, "Constant Rule");
                ec_free(orig);
            }
            return ec_num(0);
        }

        /*---- Special Functions ----*/
        case EC_FACTORIAL: {
            /* d(n!)/dx = 0 for constant n */
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = 0", orig);
                ec_derivation_add(steps, buf, "Constant Rule");
                ec_free(orig);
            }
            return ec_num(0);
        }

        case EC_ERF: {
            /* d(erf(u))/dx = 2/sqrt(pi) * exp(-u^2) * u' */
            Expr *arg     = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *u2 = ec_binary(EC_POW, ec_copy(arg), ec_num(2));
            Expr *neg_exp = ec_unary(EC_NEG, ec_unary(EC_EXP, u2));
            Expr *coef = ec_binary(EC_DIV, ec_num(2), ec_unary(EC_SQRT, ec_const(EC_PI)));
            Expr *result  = ec_binary(EC_MUL, ec_binary(EC_MUL, coef, neg_exp), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        case EC_DIGAMMA: {
            /* d(psi(u))/dx = polygamma(1, u) -- for now, leave as polygamma derivative */
            Expr *arg = e->arg;
            Expr *arg_der = diff_rec(arg, var, steps);
            Expr *result = ec_binary(EC_MUL, ec_copy(e), arg_der);
            if (steps) {
                char *orig = ec_to_latex(e);
                char *res  = ec_to_latex(result);
                char buf[512];
                snprintf(buf, sizeof(buf), "d/dx[%s] = %s", orig, res);
                ec_derivation_add(steps, buf, "Chain Rule");
                ec_free(orig); ec_free(res);
            }
            return result;
        }

        /*---- Comparison / Logical (return 0 derivative) ----*/
        case EC_LT: case EC_LE: case EC_GT: case EC_GE:
        case EC_EQ: case EC_NE:
        case EC_LAND: case EC_LOR: case EC_LNOT:
        case EC_MOD: case EC_REM: case EC_FDIM: {
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = 0  (constant w.r.t. %c)", orig, var);
                ec_derivation_add(steps, buf, "Constant Rule");
                ec_free(orig);
            }
            return ec_num(0);
        }

        /*---- Constants ----*/
        case EC_PI: case EC_E: case EC_I: case EC_INF: case EC_NINF: {
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = 0  (constant)", orig);
                ec_derivation_add(steps, buf, "Constant Rule");
                ec_free(orig);
            }
            return ec_num(0);
        }

        /*---- Fallback ----*/
        default: {
            /* For any unhandled node type, return the node unchanged */
            if (steps) {
                char buf[512];
                char *orig = ec_to_latex(e);
                snprintf(buf, sizeof(buf), "d/dx[%s] = (no rule implemented)", orig);
                ec_derivation_add(steps, buf, "Unknown");
                ec_free(orig);
            }
            return ec_copy(e);
        }
    }
}

/*============================================================
 * Public API
 *============================================================*/
Expr* ec_diff(Expr *e, char var) {
    return diff_rec(e, var, NULL);
}

Expr* ec_diff_n(Expr *e, char var, int n) {
    Expr *r = ec_copy(e);
    for (int i = 0; i < n; i++) {
        Expr *next = diff_rec(r, var, NULL);
        ec_free_expr(r);
        r = next;
    }
    return r;
}

Expr* ec_partial_diff(Expr *e, char var) {
    return diff_rec(e, var, NULL);
}

Expr* ec_total_diff(Expr *e, const char **vars, int n) {
    (void)vars; (void)n;
    return e ? ec_copy(e) : NULL;
}

/* Returns a wrapped EC_DERIV node so LaTeX renders \frac{d}{dx} correctly */
Expr* ec_diff_with_latex(Expr *e, char var) {
    Expr *d = diff_rec(e, var, NULL);
    Expr *wrapped = ec_malloc(sizeof(Expr));
    wrapped->type = EC_DERIV;
    wrapped->arg = d;
    wrapped->deriv_var = var;
    wrapped->deriv_order = 1;
    wrapped->left = wrapped->right = NULL;
    wrapped->cond = NULL;
    wrapped->var_name = 0;
    wrapped->var_str = NULL;
    wrapped->num_val = 0;
    return wrapped;
}

Derivation* ec_diff_with_steps(Expr *e, char var) {
    Derivation *steps = ec_derivation_new();
    Expr *result = diff_rec(e, var, steps);

    /* Add the initial expression as first step */
    char *latex = ec_to_latex(e);
    ec_derivation_add(steps, latex, "Initial Expression");
    ec_free(latex);

    /* Add the final result */
    latex = ec_to_latex(result);
    ec_derivation_add(steps, latex, "Final Result");
    ec_free(latex);

    ec_free_expr(result);
    return steps;
}

char* ec_diff_rule_name(ECNodeType t) {
    switch (t) {
        case EC_ADD:    return "Addition Rule";
        case EC_SUB:    return "Subtraction Rule";
        case EC_MUL:    return "Product Rule";
        case EC_DIV:    return "Quotient Rule";
        case EC_POW:    return "Power Rule";
        case EC_NEG:    return "Chain Rule";
        case EC_SIN:    return "Chain Rule";
        case EC_COS:    return "Chain Rule";
        case EC_TAN:    return "Chain Rule";
        case EC_COT:    return "Chain Rule";
        case EC_SEC:    return "Chain Rule";
        case EC_CSC:    return "Chain Rule";
        case EC_ASIN:   return "Chain Rule";
        case EC_ACOS:   return "Chain Rule";
        case EC_ATAN:   return "Chain Rule";
        case EC_SINH:   return "Chain Rule";
        case EC_COSH:   return "Chain Rule";
        case EC_TANH:   return "Chain Rule";
        case EC_EXP:    return "Chain Rule";
        case EC_LN:     return "Chain Rule";
        case EC_LOG:    return "Chain Rule";
        case EC_LOG10:  return "Chain Rule";
        case EC_SQRT:   return "Chain Rule";
        case EC_ABS:    return "Chain Rule";
        case EC_FLOOR:
        case EC_CEIL:
        case EC_ROUND:
        case EC_TRUNC:  return "Constant Rule";
        case EC_NUM:
        case EC_VAR:
        case EC_PI:
        case EC_E:
        case EC_I:
        case EC_INF:
        case EC_NINF:   return "Constant Rule";
        default:        return "Unknown";
    }
}
